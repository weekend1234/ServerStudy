#ifndef __DEFINE__
#define __DEFINE__

//----------------------------

//#undef FD_SETSIZE
//#define FD_SETSIZE 5096 // http://blog.naver.com/znfgkro1/220175848048

#ifdef _WIN32

#pragma comment(lib, "ws2_32")
#include <winsock2.h>
#include <ws2tcpip.h>

#else

#include <sys/types.h> 
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <error.h>
#include <string.h>

#define ZeroMemory(destination, length) memset((destination), 0, (length))
#define CopyMemory(Destination,Source,Length) memcpy((Destination),(Source),(Length))

typedef int SOCKET;
#define INVALID_SOCKET (~0)
#define SOCKET_ERROR (~0)
#define SOCKADDR_IN struct sockaddr_in
#define SOCKADDR struct sockaddr

#define _countof(_Array) sizeof(_Array) / sizeof(_Array[0])
#define MAX_PATH 260
#define _TRUNCATE ((size_t)-1)

#endif //_WIN32

//-------------------------------

namespace NServerNetLib
{
	struct ServerConfig
	{
		unsigned short Port;
		int BackLogCount;

		int MaxClientCount;
		int ExtraClientCount; // 가능하면 로그인에서 짜르도록 MaxClientCount + 여유분을 준비한다.
		
		short MaxClientSockOptRecvBufferSize;
		short MaxClientSockOptSendBufferSize;
		short MaxClientRecvBufferSize;
		short MaxClientSendBufferSize;

		bool IsLoginCheck;	// 연결 후 특정 시간 이내에 로그인 완료 여부 조사

		int MaxLobbyCount;
		int MaxLobbyUserCount;
		int MaxRoomCountByLobby;
		int MaxRoomUserCount;
	};

	const int MAX_IP_LEN = 32; // IP 문자열 최대 길이
	const int MAX_PACKET_BODY_SIZE = 1024; // 최대 패킷 보디 크기
	
	struct ClientSession
	{
		bool IsConnected() { return SocketFD != 0 ? true : false; }

		void Clear()
		{
			Seq = 0;
			SocketFD = 0;
			IP[0] = '\0';
			RemainingDataSize = 0;
			PrevReadPosInRecvBuffer = 0;
			SendSize = 0;
		}

		int Index = 0;
		long long Seq = 0;
		SOCKET	SocketFD = 0; //왜 SOCKET 자료형으로 안만들었을까?
		char    IP[MAX_IP_LEN] = { 0, };

		char*   pRecvBuffer = nullptr;
		int     RemainingDataSize = 0;
		int     PrevReadPosInRecvBuffer = 0;

		char*   pSendBuffer = nullptr;
		int     SendSize = 0;
	};

	struct RecvPacketInfo
	{
		int SessionIndex = 0;
		short PacketId = 0;
		short PacketBodySize = 0;
		char* pRefData = 0;
	};

	enum class SOCKET_CLOSE_CASE : short
	{
		SESSION_POOL_EMPTY = 1,
		SELECT_ERROR = 2,
		SOCKET_RECV_ERROR = 3,
		SOCKET_RECV_BUFFER_PROCESS_ERROR = 4,
		SOCKET_SEND_ERROR = 5,
		FORCING_CLOSE = 6,
	};
	

	enum class PACKET_ID : short
	{
		NTF_SYS_CONNECT_SESSION = 2,
		NTF_SYS_CLOSE_SESSION = 3,				
	};

#pragma pack(push, 1)
	struct PacketHeader
	{
		short TotalSize;
		short Id;
		unsigned char Reserve;
	};

	struct PktNtfSysCloseSession : PacketHeader
	{
		int SockFD;
	};
#pragma pack(pop)

	const int PACKET_HEADER_SIZE = sizeof(PacketHeader);
}


#endif //__DEFINE__