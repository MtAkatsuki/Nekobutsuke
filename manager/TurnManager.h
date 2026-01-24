#pragma once
#include <vector>
#include <functional>
#include "../enum class/TurnState.h"

class TurnManager {
public:
	//TurnCallBackという名前を<void(TurnPhase)>につける
	using TurnCallBack = std::function<void(TurnState)>;

	//ターンの変更知りたいときに呼び出される関数を登録する
	void RegisterObserver(TurnCallBack callback) {
		callbacksList.push_back(callback);
	}
	//二度目プレーヤーするための処理
	void ClearObservers() {
		callbacksList.clear();
	}
	//敵とプレーヤー行動終わるならSetState
	void SetState(TurnState newState) {
		currentState = newState;
		m_isTurnChangeRequest = false;//SetState実行したら、要求フラグをリセット
		NotifyObservers();//状況変化をしたUnitが状況応じて行動する
	}
	//ターン変更を要求する
	void RequestEndTurn() {
		m_isTurnChangeRequest = true;
	}
	//ターン変更が要求されているか
	bool IsTurnChangeRequested() const {
		return m_isTurnChangeRequest;
	}

	TurnState GetTurnState() const {
		return currentState;
	}

private:
	TurnState currentState = TurnState::PlayerPhase;

	std::vector<TurnCallBack> callbacksList;

	bool m_isTurnChangeRequest = false;//ターン変更要求フラグ
	
	void NotifyObservers() {
		for (const auto& func : callbacksList) {
			func(currentState);
		}
	}
};