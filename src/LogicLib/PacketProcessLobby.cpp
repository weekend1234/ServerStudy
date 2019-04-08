#include "../Common/Packet.h"
#include "../Common/ErrorCode.h"
#include "../ServerNetLib/TcpNetwork.h"
#include "User.h"
#include "UserManager.h"
#include "Lobby.h"
#include "LobbyManager.h"
#include "PacketProcess.h"

using namespace NCommon;
//using PACKET_ID = NCommon::PACKET_ID;


namespace NLogicLib
{
	ERROR_CODE PacketProcess::LobbyEnter(PacketInfo packetInfo)
	{
		auto reqPkt = (NCommon::PktLobbyEnterReq*)packetInfo.pRefData;

		auto pUserRet = m_pRefUserMgr->GetUser(packetInfo.SessionIndex);
		
		//UserManager에 실제 등록된 유저인지 체크, 로그인 했는지 체크.
		auto errorCode = std::get<0>(pUserRet);
		if (errorCode != ERROR_CODE::NONE) {
			return SetErrorPacket<PktLobbyEnterRes>(errorCode, packetInfo, PACKET_ID::LOBBY_ENTER_RES);
		}

		//로비에 들어갈 수 있는 상태인지 체크.
		auto pUser = std::get<1>(pUserRet);
		if (pUser->IsCurDomainInLogIn() == false) {
			return SetErrorPacket<PktLobbyEnterRes>(ERROR_CODE::LOBBY_ENTER_INVALID_DOMAIN, packetInfo, PACKET_ID::LOBBY_ENTER_RES);
		}

		//로비 인덱스 체크
		auto pLobby = m_pRefLobbyMgr->GetLobby(reqPkt->LobbyId);
		if (pLobby == nullptr) {
			return SetErrorPacket<PktLobbyEnterRes>(ERROR_CODE::LOBBY_ENTER_INVALID_LOBBY_INDEX, packetInfo, PACKET_ID::LOBBY_ENTER_RES);
		}

		//이미 로비안에 같은 유저가 있는지 체크
		auto enterRet = pLobby->EnterUser(pUser);
		if (enterRet != ERROR_CODE::NONE) {
			return SetErrorPacket<PktLobbyEnterRes>(enterRet, packetInfo, PACKET_ID::LOBBY_ENTER_RES);
		}

		NCommon::PktLobbyEnterRes resPkt;
		resPkt.MaxUserCount = pLobby->MaxUserCount();
		resPkt.MaxRoomCount = pLobby->MaxRoomCount();
		m_pRefNetwork->SendData(packetInfo.SessionIndex, (short)PACKET_ID::LOBBY_ENTER_RES, sizeof(NCommon::PktLobbyEnterRes), (char*)&resPkt);
		return ERROR_CODE::NONE;
	}
		
	ERROR_CODE PacketProcess::LobbyLeave(PacketInfo packetInfo)
	{
		auto pUserRet = m_pRefUserMgr->GetUser(packetInfo.SessionIndex);
		auto errorCode = std::get<0>(pUserRet);

		if (errorCode != ERROR_CODE::NONE) {
			return SetErrorPacket<PktLobbyLeaveRes>(errorCode, packetInfo, PACKET_ID::LOBBY_LEAVE_RES);
		}

		auto pUser = std::get<1>(pUserRet);
		if (pUser->IsCurDomainInLobby() == false) {
			return SetErrorPacket<PktLobbyLeaveRes>(ERROR_CODE::LOBBY_LEAVE_INVALID_DOMAIN, packetInfo, PACKET_ID::LOBBY_LEAVE_RES);
		}

		auto pLobby = m_pRefLobbyMgr->GetLobby(pUser->GetLobbyIndex());
		if (pLobby == nullptr) {
			return SetErrorPacket<PktLobbyLeaveRes>(ERROR_CODE::LOBBY_LEAVE_INVALID_LOBBY_INDEX, packetInfo, PACKET_ID::LOBBY_LEAVE_RES);
		}

		auto enterRet = pLobby->LeaveUser(pUser->GetIndex());
		if (enterRet != ERROR_CODE::NONE) {
			return SetErrorPacket<PktLobbyLeaveRes>(enterRet, packetInfo, PACKET_ID::LOBBY_LEAVE_RES);
		}
		
		NCommon::PktLobbyLeaveRes resPkt;
		m_pRefNetwork->SendData(packetInfo.SessionIndex, (short)PACKET_ID::LOBBY_LEAVE_RES, sizeof(NCommon::PktLobbyLeaveRes), (char*)&resPkt);
		return ERROR_CODE::NONE;
	}	
}