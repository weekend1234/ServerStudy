#include "../Common/Packet.h"
#include "../Common/ErrorCode.h"
#include "../ServerNetLib/TcpNetwork.h"
#include "User.h"
#include "UserManager.h"
#include "LobbyManager.h"
#include "Lobby.h"
#include "Game.h"
#include "Room.h"
#include "PacketProcess.h"

using namespace NCommon;

namespace NLogicLib
{
	ERROR_CODE PacketProcess::RoomEnter(PacketInfo packetInfo)
	{
		auto reqPkt = (NCommon::PktRoomEnterReq*)packetInfo.pRefData;

		auto pUserRet = m_pRefUserMgr->GetUser(packetInfo.SessionIndex);
		auto errorCode = std::get<0>(pUserRet);
		if (errorCode != ERROR_CODE::NONE) {
			return SetErrorPacket<PktRoomEnterRes>(errorCode, packetInfo, PACKET_ID::ROOM_ENTER_RES);
		}

		auto pUser = std::get<1>(pUserRet);
		if (pUser->IsCurDomainInLobby() == false) {
			return SetErrorPacket<PktRoomEnterRes>(ERROR_CODE::ROOM_ENTER_INVALID_DOMAIN, packetInfo, PACKET_ID::ROOM_ENTER_RES);
		}

		auto lobbyIndex = pUser->GetLobbyIndex();
		auto pLobby = m_pRefLobbyMgr->GetLobby(lobbyIndex);
		if (pLobby == nullptr) {
			return SetErrorPacket<PktRoomEnterRes>(ERROR_CODE::ROOM_ENTER_INVALID_LOBBY_INDEX, packetInfo, PACKET_ID::ROOM_ENTER_RES);
		}
		
		Room* pRoom = nullptr;
		
		// 룸을 만드는 경우라면 룸을 만든다
		if (reqPkt->IsCreate)
		{
			pRoom = pLobby->GetAvailableRoom();
			if (pRoom == nullptr) {
				return SetErrorPacket<PktRoomEnterRes>(ERROR_CODE::ROOM_ENTER_EMPTY_ROOM, packetInfo, PACKET_ID::ROOM_ENTER_RES);
			}
			else
			{
				auto ret = pRoom->CreateRoom(reqPkt->RoomTitle);
				if (ret != ERROR_CODE::NONE) {
					return SetErrorPacket<PktRoomEnterRes>(ret, packetInfo, PACKET_ID::ROOM_ENTER_RES);
				}
			}
		}
		else
		{
		    pRoom = pLobby->GetRoom(reqPkt->RoomIndex);
			if (pRoom == nullptr) {
				return SetErrorPacket<PktRoomEnterRes>(ERROR_CODE::ROOM_ENTER_INVALID_ROOM_INDEX, packetInfo, PACKET_ID::ROOM_ENTER_RES);
			}
		}

		auto enterRet = pRoom->EnterUser(pUser);
		if (enterRet != ERROR_CODE::NONE) {
			return SetErrorPacket<PktRoomEnterRes>(enterRet, packetInfo, PACKET_ID::ROOM_ENTER_RES);
		}
		
		// 유저 정보를 룸에 들어왔다고 변경한다.
		pUser->EnterRoom(lobbyIndex, pRoom->GetIndex());		
		
		// 룸에 새 유저 들어왔다고 알린다
		pRoom->NotifyEnterUserInfo(pUser->GetIndex(), pUser->GetID().c_str());
		
		NCommon::PktRoomEnterRes resPkt;
		m_pRefNetwork->SendData(packetInfo.SessionIndex, (short)PACKET_ID::ROOM_ENTER_RES, sizeof(resPkt), (char*)&resPkt);
		return ERROR_CODE::NONE;
	}

	ERROR_CODE PacketProcess::RoomLeave(PacketInfo packetInfo)
	{
		auto pUserRet = m_pRefUserMgr->GetUser(packetInfo.SessionIndex);
		auto errorCode = std::get<0>(pUserRet);

		if (errorCode != ERROR_CODE::NONE) {
			return SetErrorPacket<PktRoomLeaveRes>(errorCode, packetInfo, PACKET_ID::ROOM_LEAVE_RES);
		}

		auto pUser = std::get<1>(pUserRet);
		auto userIndex = pUser->GetIndex();

		if (pUser->IsCurDomainInRoom() == false) {
			return SetErrorPacket<PktRoomLeaveRes>(ERROR_CODE::ROOM_LEAVE_INVALID_DOMAIN, packetInfo, PACKET_ID::ROOM_LEAVE_RES);
		}

		auto lobbyIndex = pUser->GetLobbyIndex();
		auto pLobby = m_pRefLobbyMgr->GetLobby(lobbyIndex);
		if (pLobby == nullptr) {
			return SetErrorPacket<PktRoomLeaveRes>(ERROR_CODE::ROOM_ENTER_INVALID_LOBBY_INDEX, packetInfo, PACKET_ID::ROOM_LEAVE_RES);
		}

		auto pRoom = pLobby->GetRoom(pUser->GetRoomIndex());
		if (pRoom == nullptr) {
			return SetErrorPacket<PktRoomLeaveRes>(ERROR_CODE::ROOM_ENTER_INVALID_ROOM_INDEX, packetInfo, PACKET_ID::ROOM_LEAVE_RES);
		}

		auto leaveRet = pRoom->LeaveUser(userIndex);
		if (leaveRet != ERROR_CODE::NONE) {
			return SetErrorPacket<PktRoomLeaveRes>(leaveRet, packetInfo, PACKET_ID::ROOM_LEAVE_RES);
		}

		// 유저 정보를 로비로 변경
		pUser->EnterLobby(lobbyIndex);

		// 룸에 유저가 나갔음을 통보
		pRoom->NotifyLeaveUserInfo(pUser->GetID().c_str());

		NCommon::PktRoomLeaveRes resPkt;
		m_pRefNetwork->SendData(packetInfo.SessionIndex, (short)PACKET_ID::ROOM_LEAVE_RES, sizeof(resPkt), (char*)&resPkt);
		return ERROR_CODE::NONE;
	}

	ERROR_CODE PacketProcess::RoomChat(PacketInfo packetInfo)
	{
		auto reqPkt = (NCommon::PktRoomChatReq*)packetInfo.pRefData;

		auto pUserRet = m_pRefUserMgr->GetUser(packetInfo.SessionIndex);
		auto errorCode = std::get<0>(pUserRet);

		if (errorCode != ERROR_CODE::NONE) {
			return SetErrorPacket<PktRoomChatRes>(errorCode, packetInfo, PACKET_ID::ROOM_CHAT_RES);
		}

		auto pUser = std::get<1>(pUserRet);
		
		if (pUser->IsCurDomainInRoom() == false) {
			return SetErrorPacket<PktRoomChatRes>(ERROR_CODE::ROOM_CHAT_INVALID_DOMAIN, packetInfo, PACKET_ID::ROOM_CHAT_RES);
		}

		auto lobbyIndex = pUser->GetLobbyIndex();
		auto pLobby = m_pRefLobbyMgr->GetLobby(lobbyIndex);
		if (pLobby == nullptr) {
			return SetErrorPacket<PktRoomChatRes>(ERROR_CODE::ROOM_CHAT_INVALID_LOBBY_INDEX, packetInfo, PACKET_ID::ROOM_CHAT_RES);
		}

		auto pRoom = pLobby->GetRoom(pUser->GetRoomIndex());
		if (pRoom == nullptr) {
			return SetErrorPacket<PktRoomChatRes>(ERROR_CODE::ROOM_ENTER_INVALID_ROOM_INDEX, packetInfo, PACKET_ID::ROOM_CHAT_RES);
		}

		pRoom->NotifyChat(pUser->GetSessionIndex(), pUser->GetID().c_str(), reqPkt->Msg);
				
		NCommon::PktRoomChatRes resPkt;
		m_pRefNetwork->SendData(packetInfo.SessionIndex, (short)PACKET_ID::ROOM_CHAT_RES, sizeof(resPkt), (char*)&resPkt);
		return ERROR_CODE::NONE;
	}

	//TODO 아직 게임 로직은 다 구현하지 못했습니다.
	ERROR_CODE PacketProcess::RoomMasterGameStart(PacketInfo packetInfo)
	{
		PACKET_ID packet_id = PACKET_ID::ROOM_MASTER_GAME_START_RES;
		auto pUserRet = m_pRefUserMgr->GetUser(packetInfo.SessionIndex);
		auto errorCode = std::get<0>(pUserRet);

		if (errorCode != ERROR_CODE::NONE) {
			return SetErrorPacket<PktRoomMaterGameStartRes>(errorCode, packetInfo, packet_id);
		}

		auto pUser = std::get<1>(pUserRet);

		if (pUser->IsCurDomainInRoom() == false) {
			return SetErrorPacket<PktRoomMaterGameStartRes>(ERROR_CODE::ROOM_MASTER_GAME_START_INVALID_DOMAIN, packetInfo, packet_id);
		}

		auto lobbyIndex = pUser->GetLobbyIndex();
		auto pLobby = m_pRefLobbyMgr->GetLobby(lobbyIndex);
		if (pLobby == nullptr) {
			return SetErrorPacket<PktRoomMaterGameStartRes>(ERROR_CODE::ROOM_MASTER_GAME_START_INVALID_LOBBY_INDEX, packetInfo, packet_id);
		}

		auto pRoom = pLobby->GetRoom(pUser->GetRoomIndex());
		if (pRoom == nullptr) {
			return SetErrorPacket<PktRoomMaterGameStartRes>(ERROR_CODE::ROOM_MASTER_GAME_START_INVALID_ROOM_INDEX, packetInfo, packet_id);
		}

		// 방장이 맞는지 확인
		if (pRoom->IsMaster(pUser->GetIndex()) == false) {
			return SetErrorPacket<PktRoomMaterGameStartRes>(ERROR_CODE::ROOM_MASTER_GAME_START_INVALID_MASTER, packetInfo, packet_id);
		}

		// 방의 인원이 2명인가?
		if (pRoom->GetUserCount() != 2) {
			return SetErrorPacket<PktRoomMaterGameStartRes>(ERROR_CODE::ROOM_MASTER_GAME_START_INVALID_USER_COUNT, packetInfo, packet_id);
		}

		// 방의 상태가 게임을 안하는 중인지?
		if (pRoom->GetGameObj()->GetState() != GameState::NONE) {
			return SetErrorPacket<PktRoomMaterGameStartRes>(ERROR_CODE::ROOM_MASTER_GAME_START_INVALID_GAME_STATE, packetInfo, packet_id);
		}

		// 방의 게임 상태 변경
		pRoom->GetGameObj()->SetState(GameState::STARTING);
				
		// 방의 다른 유저에게 방장이 게임 시작 요청을 했음을 알리고
		pRoom->SendToAllUser((short)PACKET_ID::ROOM_MASTER_GAME_START_NTF, 
								0, 
								nullptr, 
								pUser->GetIndex());

		// 요청자에게 답변을 보낸다.
		NCommon::PktRoomMaterGameStartRes resPkt;
		m_pRefNetwork->SendData(packetInfo.SessionIndex, (short)PACKET_ID::ROOM_MASTER_GAME_START_RES, sizeof(resPkt), (char*)&resPkt);
		return ERROR_CODE::NONE;
	}

	ERROR_CODE PacketProcess::RoomGameStart(PacketInfo packetInfo)
	{
		PACKET_ID packet_id = PACKET_ID::ROOM_GAME_START_RES;
		NCommon::PktRoomGameStartRes resPkt;

		auto pUserRet = m_pRefUserMgr->GetUser(packetInfo.SessionIndex);
		auto errorCode = std::get<0>(pUserRet);

		if (errorCode != ERROR_CODE::NONE) {
			return SetErrorPacket<PktRoomGameStartRes>(errorCode, packetInfo, packet_id);
		}

		auto pUser = std::get<1>(pUserRet);

		if (pUser->IsCurDomainInRoom() == false) {
			return SetErrorPacket<PktRoomGameStartRes>(ERROR_CODE::ROOM_MASTER_GAME_START_INVALID_DOMAIN, packetInfo, packet_id);
		}

		auto lobbyIndex = pUser->GetLobbyIndex();
		auto pLobby = m_pRefLobbyMgr->GetLobby(lobbyIndex);
		if (pLobby == nullptr) {
			return SetErrorPacket<PktRoomGameStartRes>(ERROR_CODE::ROOM_MASTER_GAME_START_INVALID_LOBBY_INDEX, packetInfo, packet_id);
		}

		auto pRoom = pLobby->GetRoom(pUser->GetRoomIndex());
		if (pRoom == nullptr) {
			return SetErrorPacket<PktRoomGameStartRes>(ERROR_CODE::ROOM_MASTER_GAME_START_INVALID_ROOM_INDEX, packetInfo, packet_id);
		}

		// 방의 상태가 게임을 안하는 중인지?
		if (pRoom->GetGameObj()->GetState() != GameState::STARTING) {
			return SetErrorPacket<PktRoomGameStartRes>(ERROR_CODE::ROOM_MASTER_GAME_START_INVALID_GAME_STATE, packetInfo, packet_id);
		}

		//TODO: 이미 게임 시작 요청을 했는가?

		//TODO: 방에서 게임 시작 요청한 유저 리스트에 등록

		// 방의 다른 유저에게 게임 시작 요청을 했음을 알리고

		// 요청자에게 답변을 보낸다.
		m_pRefNetwork->SendData(packetInfo.SessionIndex, (short)PACKET_ID::ROOM_GAME_START_RES, sizeof(resPkt), (char*)&resPkt);
		
		
		// 게임 시작 가능한가?
		// 시작이면 게임 상태 변경 GameState::ING
		// 게임 시작 패킷 보내기
		// 방의 상태 변경 로비에 알리고
		// 게임의 선택 시작 시간 설정
		return ERROR_CODE::NONE;
	}
}