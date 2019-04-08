#include <algorithm>
#include "../Common/ErrorCode.h"
#include "User.h"
#include "UserManager.h"

namespace NLogicLib
{
	UserManager::UserManager()
	{
	}

	UserManager::~UserManager()
	{
	}

	void UserManager::Init(const int maxUserCount)
	{
		for (int i = 0; i < maxUserCount; ++i)
		{
			std::unique_ptr<User> user = std::make_unique<User>();
			user->Init((short)i);

			m_UserObjPool.push_back(std::move(user));
			m_UserObjPoolIndex.push_back(i);
		}
	}
	
	User* UserManager::AllocUserObjPoolIndex()
	{
		if (m_UserObjPoolIndex.empty()) {
			return nullptr;
		}

		int index = m_UserObjPoolIndex.front();
		m_UserObjPoolIndex.pop_front();
		return m_UserObjPool[index].get();
	}

	void UserManager::ReleaseUserObjPoolIndex(const int index)
	{
		m_UserObjPoolIndex.push_back(index);
		m_UserObjPool[index]->Clear();
	}

	ERROR_CODE UserManager::AddUser(const int sessionIndex, const char* pszID)
	{
		if (FindUser(pszID) != nullptr) {
			return ERROR_CODE::USER_MGR_ID_DUPLICATION;
		}

		auto pUser = AllocUserObjPoolIndex();
		if (pUser == nullptr) {
			return ERROR_CODE::USER_MGR_MAX_USER_COUNT;
		}

		pUser->Set(sessionIndex, pszID);
		
		m_UserSessionDic.insert({ sessionIndex, pUser });
		m_UserIDDic.insert({ pszID, pUser });

		return ERROR_CODE::NONE;
	}

	ERROR_CODE UserManager::RemoveUser(const int sessionIndex)
	{
		auto pUser = FindUser(sessionIndex);

		if (pUser == nullptr) {
			return ERROR_CODE::USER_MGR_REMOVE_INVALID_SESSION;
		}

		auto index = pUser->GetIndex();
		auto pszID = pUser->GetID();

		m_UserSessionDic.erase(sessionIndex);
		m_UserIDDic.erase(pszID.c_str());
		ReleaseUserObjPoolIndex(index);

		return ERROR_CODE::NONE;
	}

	std::tuple<ERROR_CODE, User*> UserManager::GetUser(const int sessionIndex)
	{
		auto pUser = FindUser(sessionIndex);

		if (pUser == nullptr) {
			return std::tuple<ERROR_CODE, User*>{ ERROR_CODE::USER_MGR_INVALID_SESSION_INDEX, nullptr };
		}

		if (pUser->IsConfirmed() == false) {
			return std::tuple<ERROR_CODE, User*>{ ERROR_CODE::USER_MGR_NOT_CONFIRM_USER, nullptr };
		}

		std::tuple<ERROR_CODE, User*> ret{ ERROR_CODE::NONE, pUser };
		return ret;
	}

	User* UserManager::FindUser(const int sessionIndex)
	{
		auto findIter = m_UserSessionDic.find(sessionIndex);
		if (findIter == m_UserSessionDic.end()) {
			return nullptr;
		}
		
		return findIter->second;
	}

	User* UserManager::FindUser(const char* pszID)
	{
		auto findIter = m_UserIDDic.find(pszID);
		if (findIter == m_UserIDDic.end()) {
			return nullptr;
		}

		return findIter->second;
	}
}