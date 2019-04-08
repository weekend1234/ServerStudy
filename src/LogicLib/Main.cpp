#include <thread>
#include <chrono>

#include "../ServerNetLib/ServerNetErrorCode.h"
#include "../ServerNetLib/Define.h"
#include "../ServerNetLib/TcpNetwork.h"
#include "ConsoleLogger.h"
#include "LobbyManager.h"
#include "PacketProcess.h"
#include "UserManager.h"
#include "Main.h"

#include "IniReader.h"

using LOG_TYPE = NServerNetLib::LOG_TYPE;
using NET_ERROR_CODE = NServerNetLib::NET_ERROR_CODE;

namespace NLogicLib
{
	Main::Main()
	{
	}

	Main::~Main()
	{
		Release();
	}

	ERROR_CODE Main::Initialize()
	{
		m_pLogger = std::make_unique<ConsoleLog>();

		auto error = LoadConfig();
		if (error != ERROR_CODE::NONE)
			return error;

		m_pLogger->Write(NServerNetLib::LOG_TYPE::L_INFO, "%s | LoadConfigSuccess.", __FUNCTION__);

		m_pNetwork = std::make_unique<NServerNetLib::TcpNetwork>();
		auto result = m_pNetwork->Init(m_pServerConfig.get(), m_pLogger.get());
		if (result != NET_ERROR_CODE::NONE)
		{
			m_pLogger->Write(LOG_TYPE::L_ERROR, "%s | Init Fail. NetErrorCode(%d)", __FUNCTION__, (short)result);
			return ERROR_CODE::MAIN_INIT_NETWORK_INIT_FAIL;
		}

		
		m_pUserMgr = std::make_unique<UserManager>();
		m_pUserMgr->Init(m_pServerConfig->MaxClientCount);

		m_pLobbyMgr = std::make_unique<LobbyManager>();
		m_pLobbyMgr->Init({ m_pServerConfig->MaxLobbyCount, 
							m_pServerConfig->MaxLobbyUserCount,
							m_pServerConfig->MaxRoomCountByLobby, 
							m_pServerConfig->MaxRoomUserCount },
						m_pNetwork.get(), m_pLogger.get());

		m_pPacketProc = std::make_unique<PacketProcess>();
		m_pPacketProc->Init(m_pNetwork.get(), m_pUserMgr.get(), m_pLobbyMgr.get(), m_pServerConfig.get(), m_pLogger.get());

		m_IsRun = true;

		m_pLogger->Write(LOG_TYPE::L_INFO, "%s | Init Success. Server Run", __FUNCTION__);
		return ERROR_CODE::NONE;
	}

	void Main::Release() 
	{
		if (m_pNetwork) {
			m_pNetwork->Release();
		}
	}

	void Main::Stop()
	{
		m_IsRun = false;
	}

	void Main::Run()
	{
		while (m_IsRun)
		{
			m_pNetwork->Run();

			while (true)
			{				
				auto packetInfo = m_pNetwork->GetPacketFromQueue();
				if (packetInfo.PacketId == 0)
				{
					break;
				}
				else
				{
					m_pPacketProc->Process(packetInfo);
				}
			}

			m_pPacketProc->StateCheck();
		}
	}

	ERROR_CODE Main::LoadConfig()
	{
		m_pServerConfig = std::make_unique<NServerNetLib::ServerConfig>();
		
		std::string filePath = "../resources/ServerConfig.ini";
#ifdef _WIN32
		filePath = "../../resources/ServerConfig.ini";
#endif
		INIReader reader(filePath);

		if (reader.ParseError() < 0) {
			m_pLogger->Write(NServerNetLib::LOG_TYPE::L_INFO, "%s | Can't load ini." , __FUNCTION__);
			return ERROR_CODE::MAIN_INIT_NETWORK_INIT_FAIL;
		}

		m_pServerConfig->Port = (unsigned short)reader.GetInteger("Config", "Port", 0);
		m_pServerConfig->BackLogCount = reader.GetInteger("Config", "BackLogCount", 0);
		m_pServerConfig->MaxClientCount = reader.GetInteger("Config", "MaxClientCount", 0);
		m_pServerConfig->MaxClientSockOptRecvBufferSize = (short)reader.GetInteger("Config", "MaxClientSockOptRecvBufferSize", 0);
		m_pServerConfig->MaxClientSockOptSendBufferSize = (short)reader.GetInteger("Config", "MaxClientSockOptSendBufferSize", 0);
		m_pServerConfig->MaxClientRecvBufferSize = (short)reader.GetInteger("Config", "MaxClientRecvBufferSize", 0);
		m_pServerConfig->MaxClientSendBufferSize = (short)reader.GetInteger("Config", "MaxClientSendBufferSize", 0);
		m_pServerConfig->IsLoginCheck = reader.GetInteger("Config", "IsLoginCheck", 0);
		m_pServerConfig->ExtraClientCount = reader.GetInteger("Config", "ExtraClientCount", 0);
		m_pServerConfig->MaxLobbyCount = reader.GetInteger("Config", "MaxLobbyCount", 0);
		m_pServerConfig->MaxLobbyUserCount = reader.GetInteger("Config", "MaxLobbyUserCount", 0);
		m_pServerConfig->MaxRoomCountByLobby = reader.GetInteger("Config", "MaxRoomCountByLobby", 0);
		m_pServerConfig->MaxRoomUserCount = reader.GetInteger("Config", "MaxRoomUserCount", 0);
		
		m_pLogger->Write(NServerNetLib::LOG_TYPE::L_INFO, "%s | Port(%d), Backlog(%d)", __FUNCTION__, m_pServerConfig->Port, m_pServerConfig->BackLogCount);
		m_pLogger->Write(NServerNetLib::LOG_TYPE::L_INFO, "%s | IsLoginCheck(%d)", __FUNCTION__, m_pServerConfig->IsLoginCheck);
		return ERROR_CODE::NONE;
	}
		
}