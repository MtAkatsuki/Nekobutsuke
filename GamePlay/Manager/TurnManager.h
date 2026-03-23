#pragma once
#include <vector>
#include <functional>
#include "../../EnumClass/TurnState.h"

// =========================================================
// TurnManager クラス
// ターン制の進行状態（フェーズ）を管理し、オブザーバーパターンを用いて
// 各エンティティ（Player, Enemy 等）へフェーズ遷移を疎結合に通知する
// =========================================================
class TurnManager {
public:
	// ターン状態の変更通知を受け取るためのコールバック型
	using TurnCallBack = std::function<void(TurnState)>;

	// ---------------------------------------------------------
	// オブザーバー管理 (Observer Management)
	// ---------------------------------------------------------
	void RegisterObserver(TurnCallBack callback) {
		m_callbacksList.push_back(callback);
	}

	void ClearObservers() {
		m_callbacksList.clear();
	}

	// ---------------------------------------------------------
	// ステート制御 (State Control)
	// ---------------------------------------------------------
	void SetState(TurnState newState) {
		m_currentState = newState;
		m_isTurnChangeRequested = false; // 状態移行が完了したため、要求フラグをリセット
		NotifyObservers();               // 登録された全オブジェクトへ状態変化を一斉通知
	}

	void RequestEndTurn() {
		m_isTurnChangeRequested = true;
	}

	// ---------------------------------------------------------
	// 状態クエリ (State Queries)
	// ---------------------------------------------------------
	bool IsTurnChangeRequested() const {
		return m_isTurnChangeRequested;
	}

	TurnState GetTurnState() const {
		return m_currentState;
	}

private:
	void NotifyObservers() {
		for (const auto& func : m_callbacksList) {
			func(m_currentState);
		}
	}

	// =========================================================
	// メンバー変数 (Member Variables)
	// =========================================================
	TurnState m_currentState = TurnState::PlayerPhase;
	std::vector<TurnCallBack> m_callbacksList;
	bool m_isTurnChangeRequested = false;
};