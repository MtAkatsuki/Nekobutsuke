#include "EnemyManager.h"
#include "../../Core/GameContext.h"
#include "TurnManager.h"
#include "../../Core/DebugLog.h"
#include "../../System/Camera.h"
#include "../../Actor/Character/Player.h"
#include <cmath>

namespace {
	const float ACT_INTERVAL = 0.5f;   // （既存）
	const float CUT_PAN_HOLD = 0.3f;  // 戦略視点で中央に配置した後の待機時間（#2 の「一瞬中央に捉える」演出）
	const float ENEMY_FOCUS_HOLD = 0.1f;   // 急降下で所定位置に到達した後、さらに待機してから行動を開始
}

void EnemyManager::Update(float deltaSeconds) {

	// フェーズの早期終了判定
	if (m_state != EnemyPhaseState::IDLE && m_state != EnemyPhaseState::ALL_FINISHED)
	{
		if (m_enemies.empty())
		{
			m_state = EnemyPhaseState::ALL_FINISHED;
			return;
		}
	}

	switch (m_state) {
	case EnemyPhaseState::IDLE:
	case EnemyPhaseState::ALL_FINISHED:
		break;

	case EnemyPhaseState::READY_TO_START:
		ResortAndRenumber();
		m_currentActorIndex = 0;
		if (!AdvanceToNextAlive()) {
			m_state = EnemyPhaseState::ALL_FINISHED;
			if (m_context->GetTurnManager()) m_context->GetTurnManager()->RequestEndTurn();
		}
		else {
			m_focusTimer = 0.0f;
			m_state = EnemyPhaseState::CUT_HOME;          //まずは戦略画面へ戻る
		}
		break;

	case EnemyPhaseState::CUT_HOME: {
		// 戦略視点へ戻る（ズームアウト中も前の対象を注視）
		Camera* cam = m_context ? m_context->GetCamera() : nullptr;
		if (!cam) { m_state = EnemyPhaseState::ACTING; break; }

		cam->HomeToStrategy();// 毎フレーム呼び出し
		if (!cam->IsCinematic() && cam->IsAtTarget()) {
			cam->SetTargetLookAt(m_enemies[m_currentActorIndex]->GetSRT().pos);
			m_focusTimer = 0.0f;
			m_state = EnemyPhaseState::CUT_PAN;
		}
		break;
	}

	case EnemyPhaseState::CUT_PAN: {

		// 戦略視点で次の対象へパン移動し、到着後に少し待機してから急降下
		Camera* cam = m_context ? m_context->GetCamera() : nullptr;

		bool arrived = (!cam) || (!cam->IsCinematic() && cam->IsAtTarget());

		if (arrived) {
			m_focusTimer += deltaSeconds;
			if (m_focusTimer >= CUT_PAN_HOLD) {
				m_focusTimer = 0.0f;
				m_enemies[m_currentActorIndex]->PlayActionOrderBounce(2); // 数字を2回バウンス
				m_state = EnemyPhaseState::CUT_BOUNCE;
			}
		}
		else {
			m_focusTimer = 0.0f;
		}
		break;
	}

	case EnemyPhaseState::CUT_BOUNCE: {
		// 戦略視点で対象を中央に捉えたまま、数字のバウンスが終了してから急降下
		if (!m_enemies[m_currentActorIndex]->IsActionOrderBouncing()) {
			m_state = EnemyPhaseState::CUT_DIVE;
		}
		break;
	}

	case EnemyPhaseState::CUT_DIVE: {
		// 対象の第三人称視点まで急降下し、完全に到達してから行動開始
		Camera* cam = m_context ? m_context->GetCamera() : nullptr;
		bool ready = (!cam) || (!cam->IsCinematic() && cam->IsAtTarget());

		if (ready) {
			m_focusTimer += deltaSeconds;
			if (m_focusTimer >= ENEMY_FOCUS_HOLD) {
				m_enemies[m_currentActorIndex]->EnemyStartAction();
				m_state = EnemyPhaseState::ACTING;
			}
		}
		else {
			m_focusTimer = 0.0f;
		}
		break;
	}

	case EnemyPhaseState::ACTING: {
		if (m_currentActorIndex >= (int)m_enemies.size()) {
			m_state = EnemyPhaseState::INTERVAL;
			m_phaseTimer = 0.0f;
			break;
		}

		Enemy* cur = m_enemies[m_currentActorIndex];
		if (cur->IsDead() || cur->IsDeadFlying() || cur->IsIdle()) {
			m_state = EnemyPhaseState::INTERVAL;
			m_phaseTimer = 0.0f;
		}
		break;
	}

	case EnemyPhaseState::INTERVAL:
		m_phaseTimer += deltaSeconds;
		if (m_phaseTimer >= ACT_INTERVAL) {
			++m_currentActorIndex;
			if (!AdvanceToNextAlive()) {
				m_state = EnemyPhaseState::ALL_FINISHED;
				if (m_context->GetTurnManager())
					m_context->GetTurnManager()->RequestEndTurn();
			}
			else {
				m_focusTimer = 0.0f;
				m_state = EnemyPhaseState::CUT_HOME;      // 次の敵も同様に、まず戦略視点へ戻る
			}
		}
		break;
	}
}

void EnemyManager::ClearAll() {
	//敵配列クリア
	m_enemies.clear();

	// ステータス リセット
	m_state = EnemyPhaseState::IDLE;
	m_currentActorIndex = 0;
	m_phaseTimer = 0.0f;
	DBG_ERROR("[EnemyManager] ClearAll called. State reset.");
}

void EnemyManager::RegisterEnemy(Enemy* enemy) {
	m_enemies.push_back(enemy);
	ResortAndRenumber();
}

void EnemyManager::RemoveEnemy(Enemy* enemy) {
	size_t oldSize = m_enemies.size();

	auto it = std::remove(m_enemies.begin(), m_enemies.end(), enemy);
	if (it != m_enemies.end()) {
		m_enemies.erase(it, m_enemies.end());
		ResortAndRenumber();
		DBG_ERROR("[Manager Debug] RemoveEnemy SUCCESS. Size: " << oldSize << " -> " << m_enemies.size());

	}
	else {
		DBG_ERROR("[Manager Debug] RemoveEnemy FAILED. Enemy pointer not found in list!");
	}
}

void EnemyManager::StartEnemyPhase() {
	m_state = EnemyPhaseState::READY_TO_START;
	m_currentActorIndex = 0;
	m_phaseTimer = 0.0f;
}

void EnemyManager::ResortAndRenumber() {
	for (int i = 0; i < m_enemies.size(); ++i) {
		if (m_enemies[i]) {
			m_enemies[i]->SetDisplayOrder(i + 1);
	}
	}

}

bool EnemyManager::AreAllEnemiesDead() const {
	for (const auto& enemy_ptr : m_enemies) {
		if (enemy_ptr->GetHP() > 0) {
			return false; // 少なくとも1体の敵が生存している
		}
	}
	return true; 
}

Enemy* EnemyManager::GetDyingEnemy() const {
	for (Enemy* enemy : m_enemies) {
		// 死亡飛翔中の敵がいるかチェック
		if (enemy && enemy->IsDeadFlying()) {
			return enemy;
		}
	}
	return nullptr;
}

bool EnemyManager::IsAnyEnemyTargeting(int gridX, int gridZ) const {
	for (Enemy* e : m_enemies) {
		if (e && e->IsCharging() && !e->IsDeadFlying() &&
			e->GetLockedGridX() == gridX && e->GetLockedGridZ() == gridZ) {
			return true;
		}
	}
	return false;
}

bool EnemyManager::IsAnyEnemyDying() const {
	return GetDyingEnemy() != nullptr;
}

bool EnemyManager::IsAnyEnemyAnimating() const {
	for (const auto* enemy : m_enemies) {
		// アクティブでアイドル状態でない敵がいるかチェック
		if (enemy->GetHP() > 0 && !enemy->IsIdle()) {
			return true;
		}
	}
	return false;
}

bool EnemyManager::AdvanceToNextAlive() {
	while (m_currentActorIndex < (int)m_enemies.size()) {
		Enemy* e = m_enemies[m_currentActorIndex];
		if (e && e->GetHP() > 0 && !e->IsDeadFlying()) return true;
		++m_currentActorIndex;
	}
	return false;
}