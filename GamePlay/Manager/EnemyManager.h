#pragma once
#include <vector>
#include <algorithm>
#include <memory>
#include "../../Actor/Character/Enemy.h"

class GameContext;

enum class EnemyPhaseState {
	IDLE,
	READY_TO_START,
	CUT_HOME,   // カメラを戦略視点へ戻す（前の対象を引き続き注視）
	CUT_PAN,   // 戦略視点でLerpに次の対象へ切り替え、中央に配置
	CUT_DIVE,   // 対象へ急降下し、第三人称視点へ移行
	CUT_BOUNCE,// 戦略視点で数字を2回バウンスさせる
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
	// 指定マスをロックオンしてチャージ中の敵がいるか（保護対象の危険警告用）
	bool IsAnyEnemyTargeting(int gridX, int gridZ) const;
	// 指定の敵がまだ登録されているか（死亡破棄後の懸垂ポインタ検出用）
	bool Contains(const Enemy* e) const {
		return std::find(m_enemies.begin(), m_enemies.end(), e) != m_enemies.end();
	}
	// 現在行動中の敵（CUT_DIVE/ACTING 中のみ有効。それ以外は nullptr）
	Enemy* GetActingEnemy() const {
		if ((m_state == EnemyPhaseState::CUT_DIVE || m_state == EnemyPhaseState::ACTING) &&
			m_currentActorIndex >= 0 && m_currentActorIndex < (int)m_enemies.size())
			return m_enemies[m_currentActorIndex];
		return nullptr;
	}

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

	// 行動開始時：プレイヤーの背後から敵を捉える構図で、戦略視点から第三人称視点へ急降下
	void FocusActor(Enemy* e);

	EnemyPhaseState m_state = EnemyPhaseState::IDLE;
	int m_currentActorIndex = 0;
	float m_phaseTimer = 0.0f;
	bool AdvanceToNextAlive();   // m_currentActorIndex から次に行動可能な敵を探し、見つからなければ false を返す
	float m_focusTimer = 0.0f;   // カメラが所定の位置に到達した後の追加待機時間
};