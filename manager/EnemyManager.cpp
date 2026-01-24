#include "EnemyManager.h"
#include "GameContext.h"
#include <iostream>

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
		std::cerr << "[Manager Debug] RemoveEnemy SUCCESS. Size: " << oldSize << " -> " << m_enemies.size() << std::endl; 
	}
	else {
		std::cerr << "[Manager Debug] RemoveEnemy FAILED. Enemy pointer not found in list!" << std::endl; 
	}
}

void EnemyManager::StartEnemyPhase() {
	m_state = EnemyPhaseState::READY_TO_START;
	m_currentActorIndex = 0;
	m_timer = 0.0f;
}

void EnemyManager::ResortAndRenumber() {
	for (int i = 0; i < m_enemies.size(); ++i) {
		if (m_enemies[i]) {
			m_enemies[i]->SetDisplayOrder(i + 1);
	}
	}

}

void EnemyManager::Update(uint64_t dt) {
	float deltaSeconds = static_cast<float>(dt) / 1000.0f;

	if (m_state != EnemyPhaseState::IDLE && m_state != EnemyPhaseState::ALL_FINISHED) 
	{
		if(m_enemies.empty())
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
		//ターン開始のとき、順番レセットする
		ResortAndRenumber();
		m_currentActorIndex = 0;
		//死んだ敵をスキップ
		while (m_currentActorIndex < m_enemies.size())
		{
			auto* enemy = m_enemies[m_currentActorIndex];
			if (enemy->GetHP() > 0 && !enemy->IsDeadFlying()) {
				break; // 生きている敵が見つかった
			}
			m_currentActorIndex++;
		}

		if (m_currentActorIndex >= m_enemies.size())
		{
			m_state = EnemyPhaseState::ALL_FINISHED;
			if (m_context->GetTurnManager());
			{ m_context->GetTurnManager()->RequestEndTurn(); }
		}
		else
		{
			m_enemies[m_currentActorIndex]->EnemyStartAction();
			m_state = EnemyPhaseState::ACTING;
		}
		break;
	case EnemyPhaseState::ACTING:
	{	//敵の削除のせいでインデックスが範囲外になる可能性がある
		if (m_currentActorIndex >= m_enemies.size()) {
			m_state = EnemyPhaseState::INTERVAL;
			m_timer = 0.0f;
			break;
		}
		Enemy* current = m_enemies[m_currentActorIndex];
		//敵は急に死亡、飛んだ、消えたら次へ
		if (current->IsDead() || current->IsDeadFlying() || current->IsIdle()) {
			m_state = EnemyPhaseState::INTERVAL;
			m_timer = 0.0f;
		}
		break;
	}
	case EnemyPhaseState::INTERVAL:
		m_timer += deltaSeconds;
		//行動後の待ち時間
		if (m_timer >= ACT_INTERVAL) {
			m_currentActorIndex++;

			while (m_currentActorIndex < m_enemies.size())
			{
				auto* enemy = m_enemies[m_currentActorIndex];
				if (enemy->GetHP() > 0 && !enemy->IsDeadFlying()) {
					break;
				}
				m_currentActorIndex++;
			}

			if (m_currentActorIndex >= m_enemies.size()) {
				//全員行動完了
				m_state = EnemyPhaseState::ALL_FINISHED;
				if (m_context->GetTurnManager()) 
				{m_context->GetTurnManager()->RequestEndTurn();}
			}
			else {
				m_enemies[m_currentActorIndex]->EnemyStartAction();
				m_state = EnemyPhaseState::ACTING;
			}
		}
			break;
	}
}

bool EnemyManager::EnemyIsAllDead() const {
	for (const auto& enemy_ptr : m_enemies) {
		if (enemy_ptr->GetHP() > 0) {
			return false; // 少なくとも1体の敵が生存している
		}
	}
	return true; // すべての敵が死亡している（すべてのifは実行しない）
}

bool EnemyManager::IsAnyEnemyDying() const {
	for (const auto* enemy : m_enemies) {
		// 死亡飛翔中の敵がいるかチェック
		if (enemy && enemy->GetHP() > 0 && enemy->IsDeadFlying()) {
			return true;
		}
	}
	return false;
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

void EnemyManager::ClearAll() {
	//敵配列クリア
	m_enemies.clear();

	// ステータス リセット
	m_state = EnemyPhaseState::IDLE;
	m_currentActorIndex = 0;
	m_timer = 0.0f;

	std::cout << "[EnemyManager] ClearAll called. State reset." << std::endl;
}