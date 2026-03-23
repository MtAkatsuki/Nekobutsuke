#pragma once

#include <memory>
#include "../Base/Unit.h"
#include "../../System/CShader.h"
#include "../../UI/Component/EnemyActionUI.h"

// =========================================================
// EnemyState
// 敵のステートマシン定義
// =========================================================
enum class EnemyState {
    IDLE,
    MOVING,
    ATTACKING,
    KNOCKBACK,
    DEAD_FLYING
};

// =========================================================
// Enemy クラス
// 独自のAIロジックと、チャージ（蓄力）攻撃のシステムを持つ敵ユニット
// =========================================================
class Enemy : public Unit {
public:
    using Unit::Unit;

    // ---------------------------------------------------------
    // ライフサイクル (Lifecycle)
    // ---------------------------------------------------------
    void init() override;
    void Init(int sequenceNumber);
    void dispose() override;
    virtual void Update(uint64_t delta) override;
    void OnDraw(uint64_t delta) override;

    // ---------------------------------------------------------
    // フロー制御 (Flow Control)
    // ---------------------------------------------------------
    void EnemyStartAction();
    virtual void OnTurnChanged(TurnState state) override;
    virtual void OnPushed(Direction pushDir) override;
    virtual void TakeDamage(int damage, Unit* attacker) override;

    // ---------------------------------------------------------
    // ステータス・クエリ (Status Queries)
    // ---------------------------------------------------------
    void SetDisplayOrder(int order) { m_displayOrder = order; }
    bool IsIdle() const { return m_state == EnemyState::IDLE; }
    bool IsDeadFlying() const { return m_state == EnemyState::DEAD_FLYING; }
    bool IsCharging() const { return m_isCharging; }
    int GetLockedGridX() const { return m_lockedGridX; }
    int GetLockedGridZ() const { return m_lockedGridZ; }
    int GetEnemyDamage() const { return m_enemyDamage; }

    void ResetCharge() {
        m_isCharging = false;
        m_pendingCharge = false;
    }

    // ---------------------------------------------------------
    // UI レンダリング (UI Rendering)
    // ---------------------------------------------------------
    virtual void DrawUI() override;

protected:
    virtual void OnDrawOverlay(uint64_t delta) override;
    virtual void OnDrawFloorUI(uint64_t delta) override;

private:
    // ---------------------------------------------------------
    // 内部AI・ロジック (Internal AI Logic)
    // ---------------------------------------------------------
    void SetInitialFacingToPlayer();
    void ExecuteAI();
    void EnemyEndAction();
    void EnemyStartMoveTo(std::vector<Tile*> path);
    void updateMove(uint64_t delta);
    void onMoveFinished();
    void StartCharge(Unit* target);
    void ReleaseChargeAttack();
    void Die();
    void DeathFlyingUpdate(float delta);

    // =========================================================
    // メンバー変数
    // =========================================================
    CShader* m_EnemyShader = nullptr;
    CStaticMeshRenderer* m_pushArrowRenderer = nullptr;
    CStaticMeshRenderer* m_attackArrowRenderer = nullptr;
    std::unique_ptr<EnemyActionUI> m_actionUI;

    EnemyState m_state = EnemyState::IDLE;

    int m_enemyDamage = 2;
    int m_displayOrder = 0;

    // --- パス・移動 ---
    std::vector<Tile*> m_currentPath;
    std::vector<Tile*> m_moveRangeTiles;
    int m_pathIndex = 0;
    Vector3 m_targetWorldPos;

    // --- 攻撃・チャージ ---
    float m_attackTimer = 0.0f;
    bool m_isMyTurn = false;
    int m_lockedGridX = -1;
    int m_lockedGridZ = -1;
    bool m_isCharging = false;
    bool m_pendingCharge = false;
    Vector3 m_hitSourcePos = Vector3(0, 0, 0);
    Vector3 m_shakeOffset = Vector3(0, 0, 0); // 【追加】レンダリング分離用の揺れオフセット

    // --- 死亡・物理 ---
    Vector3 m_deathVelocity = Vector3(0, 0, 0);
    Vector3 m_deathSpin = Vector3(0, 0, 0);
};