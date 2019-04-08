#include <stdio.h>
#include <vector>
#include <deque>

#include "ILog.h"
#include "TcpNetwork.h"


namespace NServerNetLib
{
	TcpNetwork::TcpNetwork() {}
	
	TcpNetwork::~TcpNetwork() 
	{
	}

	NET_ERROR_CODE TcpNetwork::Init(const ServerConfig* pConfig, ILog* pLogger)
	{
		memcpy(&m_Config, pConfig, sizeof(ServerConfig));

		m_pRefLogger = pLogger;

		auto initRet = InitServerSocket();
		if (initRet != NET_ERROR_CODE::NONE)
		{
			return initRet;
		}
		
		auto bindListenRet = BindListen(pConfig->Port, pConfig->BackLogCount);
		if (bindListenRet != NET_ERROR_CODE::NONE)
		{
			return bindListenRet;
		}

		FD_ZERO(&m_Readfds);
		FD_SET(m_ServerSockfd, &m_Readfds);
		
		auto sessionPoolSize = CreateSessionPool(pConfig->MaxClientCount + pConfig->ExtraClientCount);
			
		m_pRefLogger->Write(LOG_TYPE::L_INFO, "%s | Session Pool Size: %d", __FUNCTION__, sessionPoolSize);

		return NET_ERROR_CODE::NONE;
	}

	void TcpNetwork::Release()
	{
		CloseSocket(m_ServerSockfd);
		for (auto& client : m_ClientSessionPool)
		{
			if (client.IsConnected()) {
				CloseSocket(client.SocketFD);
			}

			if (client.pRecvBuffer) {
				delete[] client.pRecvBuffer;
			}

			if (client.pSendBuffer) {
				delete[] client.pSendBuffer;
			}
		}

#ifdef _WIN32
		WSACleanup();
#endif
	}

	RecvPacketInfo TcpNetwork::GetPacketFromQueue()
	{
		RecvPacketInfo packetInfo;
		if (m_PacketQueue.empty() == false)
		{
			packetInfo = m_PacketQueue.front();
			m_PacketQueue.pop_front();
		}
				
		return packetInfo;
	}
		
	void TcpNetwork::ForcingClose(const int sessionIndex)
	{
		if (m_ClientSessionPool[sessionIndex].IsConnected() == false) {
			return;
		}

		CloseSession(SOCKET_CLOSE_CASE::FORCING_CLOSE, m_ClientSessionPool[sessionIndex].SocketFD, sessionIndex);
	}

	void TcpNetwork::CloseSocket(SOCKET socket)
	{
#ifdef _WIN32
		closesocket(socket);
#else
		close(socket);
#endif
	}

	void TcpNetwork::Run()
	{
		//입력받은 FD를 제외하고 전부 0 으로 초기화되기 때문에 멤버변수->임시변수로 복사해서 사용해야 함.
		//연결된 모든 세션을 write 이벤트를 조사하고 있는데 사실 다 할 필요는 없다. 이전에 send 버퍼가 다 찼던 세션만 조사해도 된다.
		auto read_set = m_Readfds;
		auto write_set = m_Readfds;
		
		timeval timeout{ 0, 1000 }; //tv_sec, tv_usec
#ifdef _WIN32
		auto selectResult = select(0, &read_set, &write_set, 0, &timeout);
#else
		auto selectResult = select(FD_SETSIZE + 1, &read_set, &write_set, 0, &timeout);
#endif

		auto isFDSetChanged = CheckSelectResultError(selectResult);
		if (isFDSetChanged == false)
		{
			return;
		}

		// 서버소켓에 신호가 포착됐다면 Accept 호출
		if (FD_ISSET(m_ServerSockfd, &read_set))
		{
			AcceptNewSession();
		}
		
		RunCheckSelectClients(read_set, write_set);
	}

	bool TcpNetwork::CheckSelectResultError(const int result)
	{
		if (result == 0)
		{
			//select() 타임아웃
			return false;
		}
		else if (result == -1)
		{
			// TODO:로그 남기기
			return false;
		}

		return true;
	}
	
	void TcpNetwork::RunCheckSelectClients(fd_set& read_set, fd_set& write_set)
	{
		//질문:서버소켓은 걸러도 되지 않을까?, WriteSet은 언제 신호가 오는거지? 서버소켓말고 Write할 애가 있는건가?
		int size = (int)m_ClientSessionPool.size();
		for (int i = 0; i < size; ++i)
		{
			auto& session = m_ClientSessionPool[i];

			if (session.IsConnected() == false) {
				continue;
			}

			SOCKET fd = session.SocketFD;
			auto sessionIndex = session.Index;

			// check read
			auto retReceive = RunProcessReceive(sessionIndex, fd, read_set);
			if (retReceive == false) {
				continue;
			}

			// check write
			RunProcessWrite(sessionIndex, fd, write_set);
		}
	}

	bool TcpNetwork::RunProcessReceive(const int sessionIndex, const SOCKET fd, fd_set& read_set)
	{
		if (!FD_ISSET(fd, &read_set))
		{
			return true;
		}

		auto ret = RecvSocket(sessionIndex);
		if (ret != NET_ERROR_CODE::NONE)
		{
			CloseSession(SOCKET_CLOSE_CASE::SOCKET_RECV_ERROR, fd, sessionIndex);
			return false;
		}

		ret = RecvBufferProcess(sessionIndex);
		if (ret != NET_ERROR_CODE::NONE)
		{
			CloseSession(SOCKET_CLOSE_CASE::SOCKET_RECV_BUFFER_PROCESS_ERROR, fd, sessionIndex);
			return false;
		}

		return true;
	}

	int TcpNetwork::IsWouldBlocked()
	{
#ifdef _WIN32
		return WSAGetLastError() == WSAEWOULDBLOCK;
#else
		return errno == EWOULDBLOCK;
#endif
	}

	/*
	패킷을 복제하여 대상 세션의 쓰기버퍼에 담는다.
	*/
	NET_ERROR_CODE TcpNetwork::SendData(const int sessionIndex, const short packetId, const short bodySize, const char* pMsg)
	{
		auto& session = m_ClientSessionPool[sessionIndex];

		auto pos = session.SendSize;
		auto totalSize = (int16_t)(bodySize + PACKET_HEADER_SIZE);

		if ((pos + totalSize) > m_Config.MaxClientSendBufferSize ) {
			return NET_ERROR_CODE::CLIENT_SEND_BUFFER_FULL;
		}
				
		PacketHeader pktHeader{ totalSize, packetId, (uint8_t)0 };
		memcpy(&session.pSendBuffer[pos], (char*)&pktHeader, PACKET_HEADER_SIZE);

		if (bodySize > 0)
		{
			memcpy(&session.pSendBuffer[pos + PACKET_HEADER_SIZE], pMsg, bodySize);
		}

		session.SendSize += totalSize;

		return NET_ERROR_CODE::NONE;
	}

	int TcpNetwork::CreateSessionPool(const int maxClientCount)
	{
		for (int i = 0; i < maxClientCount; ++i)
		{
			ClientSession session;
			ZeroMemory(&session, sizeof(session));
			session.Index = i;
			session.pRecvBuffer = new char[m_Config.MaxClientRecvBufferSize]();
			session.pSendBuffer = new char[m_Config.MaxClientSendBufferSize]();
			
			m_ClientSessionPool.push_back(session);
			m_ClientSessionPoolIndex.push_back(session.Index);			
		}

		return maxClientCount;
	}

	int TcpNetwork::AllocClientSessionIndex()
	{
		if (m_ClientSessionPoolIndex.empty()) {
			return -1;
		}

		int index = m_ClientSessionPoolIndex.front();
		m_ClientSessionPoolIndex.pop_front();
		return index;
	}

	void TcpNetwork::ReleaseSessionIndex(const int index)
	{
		m_ClientSessionPoolIndex.push_back(index);
		m_ClientSessionPool[index].Clear();
	}

	NET_ERROR_CODE TcpNetwork::InitServerSocket()
	{
#ifdef _WIN32
		WORD wVersionRequested = MAKEWORD(2, 2);
		WSADATA wsaData;
		WSAStartup(wVersionRequested, &wsaData);
#endif

		m_ServerSockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (m_ServerSockfd < 0)
		{
			return NET_ERROR_CODE::SERVER_SOCKET_CREATE_FAIL;
		}

		auto n = 1;
		//SO_REUSEADDR : 프로그램이 종료되었지만, 비정상종료된 상태로 아직 커널이 bind정보를 유지하고 있음으로 발생하는 문제 방지.
		if (setsockopt(m_ServerSockfd, SOL_SOCKET, SO_REUSEADDR, (char*)&n, sizeof(n)) < 0)
		{
			return NET_ERROR_CODE::SERVER_SOCKET_SO_REUSEADDR_FAIL;
		}

		return NET_ERROR_CODE::NONE;
	}

	NET_ERROR_CODE TcpNetwork::BindListen(short port, int backlogCount)
	{
		SOCKADDR_IN server_addr;
		ZeroMemory(&server_addr, sizeof(server_addr));
		server_addr.sin_family = AF_INET;
		server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
		server_addr.sin_port = htons(port);

		if (bind(m_ServerSockfd, (SOCKADDR*)&server_addr, sizeof(server_addr)) < 0)
		{
			return NET_ERROR_CODE::SERVER_SOCKET_BIND_FAIL;
		}
		
		unsigned long mode = 1; //0:NonBlock, 1:Block
#ifdef _WIN32
		int ret = ioctlsocket(m_ServerSockfd, FIONBIO, &mode);
#else
		int ret = ioctl(m_ServerSockfd, FIONBIO, &mode);
#endif
		if (ret == SOCKET_ERROR)
		{
			return NET_ERROR_CODE::SERVER_SOCKET_FIONBIO_FAIL;
		}

		if (listen(m_ServerSockfd, backlogCount) == SOCKET_ERROR)
		{
			return NET_ERROR_CODE::SERVER_SOCKET_LISTEN_FAIL;
		}

		m_pRefLogger->Write(LOG_TYPE::L_INFO, "%s | Listen. ServerSockfd(%d)", __FUNCTION__, m_ServerSockfd);
		return NET_ERROR_CODE::NONE;
	}

	NET_ERROR_CODE TcpNetwork::AcceptNewSession()
	{
		//질문: 왜 do while로 accept를 반복할까?

		auto tryCount = 0; // 너무 많이 accept를 시도하지 않도록 한다.

		do
		{
			++tryCount;

			SOCKADDR_IN client_addr;
#ifdef _WIN32
			int client_addr_len = static_cast<int>(sizeof(client_addr));
#else
			socklen_t client_addr_len = sizeof(client_addr);
#endif
			auto client_sockfd = accept(m_ServerSockfd, (SOCKADDR*)&client_addr, &client_addr_len);
			//m_pRefLogger->Write(LOG_TYPE::L_DEBUG, "%s | client_sockfd(%I64u)", __FUNCTION__, client_sockfd);
			if (client_sockfd == INVALID_SOCKET)
			{
				if (IsWouldBlocked()) //연결 요청 받지 못함
				{
					return NET_ERROR_CODE::ACCEPT_API_WSAEWOULDBLOCK;
				}

				m_pRefLogger->Write(LOG_TYPE::L_ERROR, "%s | Wrong socket cannot accept", __FUNCTION__);
				return NET_ERROR_CODE::ACCEPT_API_ERROR;
			}

			auto newSessionIndex = AllocClientSessionIndex();
			if (newSessionIndex < 0)
			{
				m_pRefLogger->Write(LOG_TYPE::L_WARN, "%s | client_sockfd(%I64u)  >= MAX_SESSION", __FUNCTION__, client_sockfd);

				// 더 이상 수용할 수 없으므로 바로 끊어버린다.
				CloseSession(SOCKET_CLOSE_CASE::SESSION_POOL_EMPTY, client_sockfd, -1);
				return NET_ERROR_CODE::ACCEPT_MAX_SESSION_COUNT;
			}


			char clientIP[MAX_IP_LEN] = { 0, };
			inet_ntop(AF_INET, &(client_addr.sin_addr), clientIP, MAX_IP_LEN - 1);

			SetClientSockOption(client_sockfd);

			FD_SET(client_sockfd, &m_Readfds);
			//m_pRefLogger->Write(LOG_TYPE::L_DEBUG, "%s | client_sockfd(%I64u)", __FUNCTION__, client_sockfd);
			ConnectedSession(newSessionIndex, client_sockfd, clientIP);

		} while (tryCount < FD_SETSIZE);
		
		return NET_ERROR_CODE::NONE;
	}
	
	/*
	세션풀에 클라이언트를 새로 등록, 패킷큐에 연결패킷 추가
	*/
	void TcpNetwork::ConnectedSession(const int sessionIndex, const SOCKET fd, const char* pIP)
	{
		++m_ConnectSeq; //질문:Seq랑 m_ConnectedSessionCount 는 왜 나눠져 있는걸까?

		auto& session = m_ClientSessionPool[sessionIndex];
		session.Seq = m_ConnectSeq;
		session.SocketFD = fd;
		memcpy(session.IP, pIP, MAX_IP_LEN - 1);

		++m_ConnectedSessionCount;

		AddPacketQueue(sessionIndex, (short)PACKET_ID::NTF_SYS_CONNECT_SESSION, 0, nullptr);
		m_pRefLogger->Write(LOG_TYPE::L_INFO, "%s | New Session. FD(%I64u), m_ConnectSeq(%d), IP(%s)", __FUNCTION__, fd, m_ConnectSeq, pIP);
	}

	void TcpNetwork::SetClientSockOption(const SOCKET fd)
	{
		// LINGER옵션
		// close 함수를 호출할 때 커널에서 응용 프로그램으로 복귀하는 시점을 송신 버퍼의 자료가 모두 전송된 것이 확인될 때까지 지연할 수 있다.
		linger ling = { 0, 0 };
		setsockopt(fd, SOL_SOCKET, SO_LINGER, (char*)&ling, sizeof(ling));

		int size1 = m_Config.MaxClientSockOptRecvBufferSize;
		int size2 = m_Config.MaxClientSockOptSendBufferSize;
		setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (char*)&size1, sizeof(size1));
		setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (char*)&size2, sizeof(size2));
	}

	/*
	closesocket, FD삭제, ReleaseSessionIndex, 패킷 추가
	*/
	void TcpNetwork::CloseSession(const SOCKET_CLOSE_CASE closeCase, const SOCKET sockFD, const int sessionIndex)
	{
		if (closeCase == SOCKET_CLOSE_CASE::SESSION_POOL_EMPTY)
		{
			CloseSocket(sockFD);
			FD_CLR(sockFD, &m_Readfds);
			return;
		}

		if (m_ClientSessionPool[sessionIndex].IsConnected() == false) {
			return;
		}

		CloseSocket(sockFD);
		FD_CLR(sockFD, &m_Readfds);

		m_ClientSessionPool[sessionIndex].Clear();
		--m_ConnectedSessionCount;
		ReleaseSessionIndex(sessionIndex);

		AddPacketQueue(sessionIndex, (short)PACKET_ID::NTF_SYS_CLOSE_SESSION, 0, nullptr);
	}

	/* 
	recv()함수 호출
	*/
	NET_ERROR_CODE TcpNetwork::RecvSocket(const int sessionIndex)
	{
		auto& session = m_ClientSessionPool[sessionIndex];
		if (session.IsConnected() == false)
		{
			return NET_ERROR_CODE::RECV_PROCESS_NOT_CONNECTED;
		}

		int recvPos = 0;
				
		if (session.RemainingDataSize > 0)
		{
			memcpy(session.pRecvBuffer, &session.pRecvBuffer[session.PrevReadPosInRecvBuffer], session.RemainingDataSize);
			recvPos += session.RemainingDataSize;
		}

		//질문:왜 (MAX_PACKET_BODY_SIZE * 2) 하는걸까?
		auto fd = static_cast<SOCKET>(session.SocketFD);
		auto recvSize = recv(fd, &session.pRecvBuffer[recvPos], (MAX_PACKET_BODY_SIZE * 2), 0);
		if (recvSize == 0)
		{
			return NET_ERROR_CODE::RECV_REMOTE_CLOSE;
		}

		if (recvSize < 0)
		{
			if (IsWouldBlocked())
			{
				return NET_ERROR_CODE::RECV_API_ERROR; 
			}
			else 
			{
				// recv WSAEWOULDBLOCK : 데이터 아직 못받음
				return NET_ERROR_CODE::NONE;
			}
		}

		session.RemainingDataSize += (int)recvSize;
		return NET_ERROR_CODE::NONE;
	}

	/*
	버퍼 사이즈가 패킷 헤더보다 사이즈가 크다면 패킷을 디코딩하여 패킷을 큐에 넣음.
	사이즈가 헤더보다 작다면 이어서 게속 데이타를 받을 수 있도록 함.
	*/
	NET_ERROR_CODE TcpNetwork::RecvBufferProcess(const int sessionIndex)
	{
		auto& session = m_ClientSessionPool[sessionIndex];
		
		auto readPos = 0;
		const auto dataSize = session.RemainingDataSize;
		PacketHeader* pPktHeader;
		
		int curRemainDataSize = (dataSize - readPos);
		while (curRemainDataSize >= PACKET_HEADER_SIZE)
		{
			pPktHeader = (PacketHeader*)&session.pRecvBuffer[readPos];
			readPos += PACKET_HEADER_SIZE;
			
			auto bodySize = (int16_t)(pPktHeader->TotalSize - PACKET_HEADER_SIZE);
			if (bodySize > 0)
			{
				//헤더는 읽었지만 body를 읽기에 모자란 경우
				if (bodySize > curRemainDataSize)
				{
					readPos -= PACKET_HEADER_SIZE;
					break;
				}

				//최대 패킷 사이즈보다 큰 경우, 뭔가 설계에 문제가 있거나 오류
				if (bodySize > MAX_PACKET_BODY_SIZE)
				{
					// 더 이상 이 세션과는 작업을 하지 않을 예정. 클라이언트 보고 나가라고 하던가 직접 짤라야 한다.
					return NET_ERROR_CODE::RECV_CLIENT_MAX_PACKET;
				}
			}

			AddPacketQueue(sessionIndex, pPktHeader->Id, bodySize, &session.pRecvBuffer[readPos]);
			readPos += bodySize;
			curRemainDataSize = (dataSize - readPos);
		}
		
		session.RemainingDataSize -= readPos;
		session.PrevReadPosInRecvBuffer = readPos;
		
		return NET_ERROR_CODE::NONE;
	}

	void TcpNetwork::AddPacketQueue(const int sessionIndex, const short pktId, const short bodySize, char* pDataPos)
	{
		RecvPacketInfo packetInfo;
		packetInfo.SessionIndex = sessionIndex;
		packetInfo.PacketId = pktId;
		packetInfo.PacketBodySize = bodySize;
		packetInfo.pRefData = pDataPos;

		m_PacketQueue.push_back(packetInfo);
	}

	void TcpNetwork::RunProcessWrite(const int sessionIndex, const SOCKET fd, fd_set& write_set)
	{
		if (!FD_ISSET(fd, &write_set))
		{
			return;
		}

		auto resultSend = FlushSendBuff(sessionIndex);
		if (resultSend.Error != NET_ERROR_CODE::NONE)
		{
			m_pRefLogger->Write(LOG_TYPE::L_ERROR, "%s | send error %d", __FUNCTION__, resultSend.Value);
			CloseSession(SOCKET_CLOSE_CASE::SOCKET_SEND_ERROR, fd, sessionIndex);
		}
	}

	/*
	send 호출 후 한번의 전송으로 데이타가 전부 보내지지 않은 경우 처리
	*/
	NetError TcpNetwork::FlushSendBuff(const int sessionIndex)
	{
		auto& session = m_ClientSessionPool[sessionIndex];
		auto fd = static_cast<SOCKET>(session.SocketFD);

		if (session.IsConnected() == false)
		{
			return NetError(NET_ERROR_CODE::CLIENT_FLUSH_SEND_BUFF_REMOTE_CLOSE);
		}

		auto result = SendSocket(fd, session.pSendBuffer, session.SendSize);
		if (result.Error != NET_ERROR_CODE::NONE) {
			return result;
		}

		//보낼 데이타가 남았는지 검사 후 처리.
		//session.SendSize에는 최초에 Total Send Size가 담긴다.
		auto sendSize = result.Value;
		if (sendSize < session.SendSize)
		{
			memmove(&session.pSendBuffer[0],
				&session.pSendBuffer[sendSize],
				session.SendSize - sendSize);

			session.SendSize -= sendSize;
		}
		else
		{
			session.SendSize = 0;
		}
		return result;
	}

	/*
	send() 호출
	*/
	NetError TcpNetwork::SendSocket(const SOCKET fd, const char* pMsg, const int size)
	{
		NetError result(NET_ERROR_CODE::NONE);

		// 접속 되어 있는지 또는 보낼 데이터가 있는지
		if (size <= 0)
		{
			return result;
		}

		//send가 size를 리턴하지만 동시에 실패할때는 에러코드 리턴
		result.Value = (int)send(fd, pMsg, size, 0);
		if (result.Value <= 0)
		{
			result.Error = NET_ERROR_CODE::SEND_SIZE_ZERO;
		}

		return result;
	}

	
}