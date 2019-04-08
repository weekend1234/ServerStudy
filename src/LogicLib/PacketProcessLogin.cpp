#include "../Common/Packet.h"
#include "../Common/ErrorCode.h"
#include "../ServerNetLib/TcpNetwork.h"
#include "ConnectedUserManager.h"
#include "User.h"
#include "UserManager.h"
#include "LobbyManager.h"
#include "PacketProcess.h"

using namespace NCommon;

namespace NLogicLib
{
	ERROR_CODE PacketProcess::Login(PacketInfo packetInfo)
	{
		//TODO: 받은 데이터가 PktLogInReq 크기만큼인지 조사해야 한다.
		// 패스워드는 무조건 pass 해준다.
		
		auto reqPkt = (NCommon::PktLogInReq*)packetInfo.pRefData;

		// ID 중복이거나 최대 유저를 넘어간다면 에러 처리.
		auto addRet = m_pRefUserMgr->AddUser(packetInfo.SessionIndex, reqPkt->szID);
		if (addRet != ERROR_CODE::NONE) {
			return SetErrorPacket<PktLogInRes>(addRet, packetInfo, PACKET_ID::LOGIN_IN_RES);
		}

		m_pConnectedUserManager->SetLogin(packetInfo.SessionIndex);

		PktLogInRes resPkt;
		resPkt.ErrorCode = (short)addRet;
		m_pRefNetwork->SendData(packetInfo.SessionIndex, (short)PACKET_ID::LOGIN_IN_RES, sizeof(NCommon::PktLogInRes), (char*)&resPkt);
		return ERROR_CODE::NONE;
	}

	ERROR_CODE PacketProcess::LobbyList(PacketInfo packetInfo)
	{
		std::tuple<ERROR_CODE, User*> pUserRet = m_pRefUserMgr->GetUser(packetInfo.SessionIndex);
		
		// 인증 받은 유저인가?
		auto errorCode = std::get<0>(pUserRet);
		if (errorCode != ERROR_CODE::NONE) {
			return SetErrorPacket<PktLobbyListRes>(errorCode, packetInfo, PACKET_ID::LOBBY_LIST_RES);
		}
	
		// 아직 로그인 하지 않은 유저인가?
		auto pUser = std::get<1>(pUserRet);
		if (pUser->IsCurDomainInLogIn() == false) {
			return SetErrorPacket<PktLobbyListRes>(ERROR_CODE::LOBBY_LIST_INVALID_DOMAIN, packetInfo, PACKET_ID::LOBBY_LIST_RES);
		}
		
		m_pRefLobbyMgr->SendLobbyListInfo(packetInfo.SessionIndex);
		return ERROR_CODE::NONE;
	}
}