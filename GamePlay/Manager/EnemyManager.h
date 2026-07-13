#pragma once
#include <vector>
#include <algorithm>
#include <memory>
#include "../../Actor/Character/Enemy.h"

class GameContext;

enum class EnemyPhaseState {
	IDLE,
	READY_TO_START,
	ACTING,
	INTERVAL,
	ALL_FINISHED
};

// =========================================================
// EnemyManager クラス
// 敵フェーズにおけるAIの行動順序と状態遷移を管理する
// =========================================================
class EnemyManager {
public:
	EnemyManager() = default;
	~EnemyManager() = default;

	// ---------------------------------------------------------
	// ライフサイクルと初期化 (Lifecycle & Initialization)
	// ---------------------------------------------------------
	void Init(GameContext* context) { m_context = context; }
	// 敵フェーズの状態機械を進め、敵を 1 体ずつ順に行動させる
	void Update(float deltaSeconds);
	void ClearAll();

	// ---------------------------------------------------------
	// エンティティ管理 (Entity Management)
	// ---------------------------------------------------------
	void RegisterEnemy(Enemy* enemy);
	void RemoveEnemy(Enemy* enemy);
	const std::vector<Enemy*>& GetAllEnemies() const { return m_enemies; }

	// ---------------------------------------------------------
	// フェーズ制御 (Phase Control)
	// ---------------------------------------------------------
	// 敵フェーズを開始し、先頭の敵から行動させる準備をする
	void StartEnemyPhase();

	// ---------------------------------------------------------
	// 状態クエリ (State Queries)
	// ---------------------------------------------------------
	bool IsFinished() const { return m_state == EnemyPhaseState::ALL_FINISHED; }
	bool AreAllEnemiesDead() const;                             
	bool IsEnemyListEmpty() const { return m_enemies.empty(); } 
	bool IsAnyEnemyDying() const;
	bool IsAnyEnemyAnimating() const;
	Enemy* GetDyingEnemy() const;   // 死亡演出中の敵を返す（存在しない場合は nullptr）

private:
	// ---------------------------------------------------------
	// 内部サブルーチン (Internal Sub-routines)
	// ---------------------------------------------------------
	// 各敵に行動順の表示番号を振り直す
	void ResortAndRenumber();

	// =========================================================
	// メンバー変数 (Member Variables)
	// =========================================================
	GameContext* m_context = nullptr;
	std::vector<Enemy*> m_enemies;

	EnemyPhaseState m_state = EnemyPhaseState::IDLE;
	int m_currentActorIndex = 0;
	float m_phaseTimer = 0.0f;
};