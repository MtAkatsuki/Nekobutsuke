#include "EnemyManager.h"
#include "../../Core/GameContext.h"
#include "TurnManager.h"
#include "../../Core/DebugLog.h"

namespace {
	const float ACT_INTERVAL = 0.5f; // 敵の行動と行動の間のインターバル待機時間
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

		// 行動可能な敵を検索
		while (m_currentActorIndex < m_enemies.size()) {
			auto* enemy = m_enemies[m_currentActorIndex];
			if (enemy->GetHP() > 0 && !enemy->IsDeadFlying()) break;
			++m_currentActorIndex;
		}

		if (m_currentActorIndex >= m_enemies.size()) {
			m_state = EnemyPhaseState::ALL_FINISHED;
			if (m_context->GetTurnManager()) m_context->GetTurnManager()->RequestEndTurn();
		}
		else {
			m_enemies[m_currentActorIndex]->EnemyStartAction();
			m_state = EnemyPhaseState::ACTING;
		}
		break;

	case EnemyPhaseState::ACTING: {
		if (m_currentActorIndex >= m_enemies.size()) {
			m_state = EnemyPhaseState::INTERVAL;
			m_phaseTimer = 0.0f;
			break;
		}
		Enemy* current = m_enemies[m_currentActorIndex];
		// アニメーション完了、または急な死亡によるフェーズ進行の遮断解除
		if (current->IsDead() || current->IsDeadFlying() || current->IsIdle()) {
			m_state = EnemyPhaseState::INTERVAL;
			m_phaseTimer = 0.0f;
		}
		break;
	}

	case EnemyPhaseState::INTERVAL:
		m_phaseTimer += deltaSeconds;

		//行動後の待ち時間
		if (m_phaseTimer >= ACT_INTERVAL) {
			++m_currentActorIndex;

			while (m_currentActorIndex < m_enemies.size()) {
				auto* enemy = m_enemies[m_currentActorIndex];
				if (enemy->GetHP() > 0 && !enemy->IsDeadFlying()) break;
				++m_currentActorIndex;
			}

			if (m_currentActorIndex >= m_enemies.size()) {
				m_state = EnemyPhaseState::ALL_FINISHED;
				if (m_context->GetTurnManager()) m_context->GetTurnManager()->RequestEndTurn();
			}
			else {
				m_enemies[m_currentActorIndex]->EnemyStartAction();
				m_state = EnemyPhaseState::ACTING;
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

