#pragma once
#include <vector>
#include <functional>
#include "../../Types/TurnState.h"

// =========================================================
// TurnManager クラス
// ターン制の進行状態（フェーズ）を管理し、オブザーバーパターンを用いて
// 各エンティティ（Player, Enemy 等）へフェーズ遷移を疎結合に通知する
// =========================================================
class TurnManager {
public:
	// ターン状態の変更通知を受け取るためのコールバック型
	using TurnCallBack = std::function<void(TurnState)>;
	using ObserverId = size_t;
	// ---------------------------------------------------------
	// オブザーバー管理 (Observer Management)
	// ---------------------------------------------------------
	// 戻り値の ObserverId を保存し、購読者の破棄時に UnregisterObserver で解除する
	ObserverId RegisterObserver(TurnCallBack callback) {
		m_callbacksList.push_back({ ++m_nextObserverId, std::move(callback) });
		return m_nextObserverId;
	}

	// ※ NotifyObservers の走査中に呼んではならない。
	//   現状の削除経路は GameScene::RemoveDeadObjects → ~Unit のみで、通知中には走らない。
	void UnregisterObserver(ObserverId id) {
		std::erase_if(m_callbacksList, [id](const Entry& e) { return e.id == id; });
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
	struct Entry {
		ObserverId id;
		TurnCallBack callback;
	};

	void NotifyObservers() {
		for (const auto& e : m_callbacksList) {
			e.callback(m_currentState);
		}
	}

	// =========================================================
	// メンバー変数 (Member Variables)
	// =========================================================
	TurnState m_currentState = TurnState::PlayerPhase;
	std::vector<Entry> m_callbacksList;
	ObserverId m_nextObserverId = 0;
	bool m_isTurnChangeRequested = false;
};