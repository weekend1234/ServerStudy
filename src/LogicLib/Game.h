#pragma once

namespace NLogicLib
{
	enum class GameState 
	{
		NONE,
		STARTING,
		ING,
		END
	};

	class Game
	{
	public:
		Game() {}
		virtual ~Game() {}

		void Clear();
		GameState GetState() { return m_State;  }
		void SetState(const GameState state) { m_State = state; }
		bool CheckSelectTime();

	private:
		GameState m_State = GameState::NONE;

		int64_t m_SelectTime;
		int m_GameSelect1; // 가위(0), 바위(1), 보(2)
		int m_GameSelect2;
	};

}