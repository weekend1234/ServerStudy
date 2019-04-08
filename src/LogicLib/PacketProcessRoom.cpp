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
		
		// ���� ����� ����� ���� �����
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
		
		// ���� ������ �뿡 ���Դٰ� �����Ѵ�.
		pUser->EnterRoom(lobbyIndex, pRoom->GetIndex());		
		
		// �뿡 �� ���� ���Դٰ� �˸���
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

		// ���� ������ �κ�� ����
		pUser->EnterLobby(lobbyIndex);

		// �뿡 ������ �������� �뺸
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

	//TODO ���� ���� ������ �� �������� ���߽��ϴ�.
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

		// ������ �´��� Ȯ��
		if (pRoom->IsMaster(pUser->GetIndex()) == false) {
			return SetErrorPacket<PktRoomMaterGameStartRes>(ERROR_CODE::ROOM_MASTER_GAME_START_INVALID_MASTER, packetInfo, packet_id);
		}

		// ���� �ο��� 2���ΰ�?
		if (pRoom->GetUserCount() != 2) {
			return SetErrorPacket<PktRoomMaterGameStartRes>(ERROR_CODE::ROOM_MASTER_GAME_START_INVALID_USER_COUNT, packetInfo, packet_id);
		}

		// ���� ���°� ������ ���ϴ� ������?
		if (pRoom->GetGameObj()->GetState() != GameState::NONE) {
			return SetErrorPacket<PktRoomMaterGameStartRes>(ERROR_CODE::ROOM_MASTER_GAME_START_INVALID_GAME_STATE, packetInfo, packet_id);
		}

		// ���� ���� ���� ����
		pRoom->GetGameObj()->SetState(GameState::STARTING);
				
		// ���� �ٸ� �������� ������ ���� ���� ��û�� ������ �˸���
		pRoom->SendToAllUser((short)PACKET_ID::ROOM_MASTER_GAME_START_NTF, 
								0, 
								nullptr, 
								pUser->GetIndex());

		// ��û�ڿ��� �亯�� ������.
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

		// ���� ���°� ������ ���ϴ� ������?
		if (pRoom->GetGameObj()->GetState() != GameState::STARTING) {
			return SetErrorPacket<PktRoomGameStartRes>(ERROR_CODE::ROOM_MASTER_GAME_START_INVALID_GAME_STATE, packetInfo, packet_id);
		}

		//TODO: �̹� ���� ���� ��û�� �ߴ°�?

		//TODO: �濡�� ���� ���� ��û�� ���� ����Ʈ�� ���

		// ���� �ٸ� �������� ���� ���� ��û�� ������ �˸���

		// ��û�ڿ��� �亯�� ������.
		m_pRefNetwork->SendData(packetInfo.SessionIndex, (short)PACKET_ID::ROOM_GAME_START_RES, sizeof(resPkt), (char*)&resPkt);
		
		
		// ���� ���� �����Ѱ�?
		// �����̸� ���� ���� ���� GameState::ING
		// ���� ���� ��Ŷ ������
		// ���� ���� ���� �κ� �˸���
		// ������ ���� ���� �ð� ����
		return ERROR_CODE::NONE;
	}
}