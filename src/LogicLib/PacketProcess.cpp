#include "../ServerNetLib/ILog.h"
#include "../ServerNetLib/TcpNetwork.h"
#include "ConnectedUserManager.h"
#include "User.h"
#include "UserManager.h"
#include "Room.h"
#include "Lobby.h"
#include "LobbyManager.h"
#include "PacketProcess.h"

using LOG_TYPE = NServerNetLib::LOG_TYPE;
using ServerConfig = NServerNetLib::ServerConfig;

namespace NLogicLib
{	
	PacketProcess::PacketProcess() {}
	PacketProcess::~PacketProcess() {}

	void PacketProcess::Init(TcpNet* pNetwork, UserManager* pUserMgr, LobbyManager* pLobbyMgr, ServerConfig* pConfig, ILog* pLogger)
	{
		m_pRefLogger = pLogger;
		m_pRefNetwork = pNetwork;
		m_pRefUserMgr = pUserMgr;
		m_pRefLobbyMgr = pLobbyMgr;

		m_pConnectedUserManager = std::make_unique<ConnectedUserManager>();
		m_pConnectedUserManager->Init(pNetwork->ClientSessionPoolSize(), pNetwork, pConfig, pLogger);

		using netLib = NServerNetLib::PACKET_ID;
		using common = NCommon::PACKET_ID;
		
		for (int i = 0; i < (int)common::MAX; ++i)
		{
			PacketFuncArray[i] = nullptr;
		}

		#define PACKET_FUNCTION_BIND(funcName) std::bind(&PacketProcess::funcName, this, std::placeholders::_1)

		PacketFuncArray[(int)netLib::NTF_SYS_CONNECT_SESSION] = PACKET_FUNCTION_BIND(NtfSysConnctSession);
		PacketFuncArray[(int)netLib::NTF_SYS_CLOSE_SESSION] = PACKET_FUNCTION_BIND(NtfSysCloseSession);
		
		PacketFuncArray[(int)common::LOGIN_IN_REQ] = PACKET_FUNCTION_BIND(Login);
		PacketFuncArray[(int)common::LOBBY_LIST_REQ] = PACKET_FUNCTION_BIND(LobbyList);
		PacketFuncArray[(int)common::LOBBY_ENTER_REQ] = PACKET_FUNCTION_BIND(LobbyEnter);
		PacketFuncArray[(int)common::LOBBY_LEAVE_REQ] = PACKET_FUNCTION_BIND(LobbyLeave);

		PacketFuncArray[(int)common::ROOM_ENTER_REQ] = PACKET_FUNCTION_BIND(RoomEnter);
		PacketFuncArray[(int)common::ROOM_LEAVE_REQ] = PACKET_FUNCTION_BIND(RoomLeave);
		PacketFuncArray[(int)common::ROOM_CHAT_REQ] = PACKET_FUNCTION_BIND(RoomChat);
		PacketFuncArray[(int)common::ROOM_MASTER_GAME_START_REQ] = PACKET_FUNCTION_BIND(RoomMasterGameStart);
		PacketFuncArray[(int)common::ROOM_GAME_START_REQ] = PACKET_FUNCTION_BIND(RoomGameStart);

		PacketFuncArray[(int)common::DEV_ECHO_REQ] = PACKET_FUNCTION_BIND(PacketProcess::DevEcho);
	}
	
	void PacketProcess::Process(PacketInfo packetInfo)
	{
		auto packetId = packetInfo.PacketId;
		if (PacketFuncArray[packetId] == nullptr)
		{
			m_pRefLogger->Write(LOG_TYPE::L_ERROR, "%s | Connected packet function is null.", __FUNCTION__, packetInfo.PacketId);
			return;
		}

		m_pRefLogger->Write(LOG_TYPE::L_INFO, "%s | Process Packet : %d ", __FUNCTION__, packetInfo.PacketId);
		PacketFuncArray[packetId](packetInfo);
	}

	void PacketProcess::StateCheck()
	{
		m_pConnectedUserManager->LoginCheck();
	}

	ERROR_CODE PacketProcess::NtfSysConnctSession(PacketInfo packetInfo)
	{
		m_pConnectedUserManager->SetConnectSession(packetInfo.SessionIndex);
		return ERROR_CODE::NONE;
	}

	ERROR_CODE PacketProcess::NtfSysCloseSession(PacketInfo packetInfo)
	{
		if (auto pUser = std::get<1>(m_pRefUserMgr->GetUser(packetInfo.SessionIndex)))
		{
			if (auto pLobby = m_pRefLobbyMgr->GetLobby(pUser->GetLobbyIndex()))
			{
				if (auto pRoom = pLobby->GetRoom(pUser->GetRoomIndex()))
				{
					pRoom->LeaveUser(pUser->GetIndex());
					pRoom->NotifyLeaveUserInfo(pUser->GetID().c_str());
					m_pRefLogger->Write(LOG_TYPE::L_INFO, "%s | NtfSysCloseSesson. sessionIndex(%d). Room Out", __FUNCTION__, packetInfo.SessionIndex);
				}

				pLobby->LeaveUser(pUser->GetIndex());
				m_pRefLogger->Write(LOG_TYPE::L_INFO, "%s | NtfSysCloseSesson. sessionIndex(%d). Lobby Out", __FUNCTION__, packetInfo.SessionIndex);
			}
			
			m_pRefUserMgr->RemoveUser(packetInfo.SessionIndex);		
		}
		
		m_pConnectedUserManager->SetDisConnectSession(packetInfo.SessionIndex);

		m_pRefLogger->Write(LOG_TYPE::L_INFO, "%s | NtfSysCloseSesson. sessionIndex(%d)", __FUNCTION__, packetInfo.SessionIndex);
		return ERROR_CODE::NONE;
	}
	

	ERROR_CODE PacketProcess::DevEcho(PacketInfo packetInfo)
	{		
		auto reqPkt = (NCommon::PktDevEchoReq*)packetInfo.pRefData;
		
		NCommon::PktDevEchoRes resPkt;
		resPkt.ErrorCode = (short)ERROR_CODE::NONE;
		resPkt.DataSize = reqPkt->DataSize;
		CopyMemory(resPkt.Datas, reqPkt->Datas, reqPkt->DataSize);
		
		auto sendSize = sizeof(NCommon::PktDevEchoRes) - (NCommon::DEV_ECHO_DATA_MAX_SIZE - reqPkt->DataSize);
		m_pRefNetwork->SendData(packetInfo.SessionIndex, (short)NCommon::PACKET_ID::DEV_ECHO_RES, (short)sendSize, (char*)&resPkt);

		return ERROR_CODE::NONE;
	}
}