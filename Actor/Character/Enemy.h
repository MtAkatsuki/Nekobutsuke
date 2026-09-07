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
    DEATH_SHAKE,
    DEAD_FLYING
};

// =========================================================
// Enemy クラス
// 独自のAIロジックと、チャージ（蓄力）攻撃のシステムを持つ敵ユニット
// =========================================================
class Enemy : public Unit {
public:
    // ---------------------------------------------------------
    // ライフサイクル (Lifecycle)
    // ---------------------------------------------------------
    static std::unique_ptr<Enemy> Spawn(GameContext* ctx, int gridX, int gridZ, const Vector3& worldPos);
 
    void Dispose() override {}
    virtual void Update(float deltaSeconds) override;
    void OnDraw(float deltaSeconds) override;

    // ---------------------------------------------------------
    // フロー制御 (Flow Control)
    // ---------------------------------------------------------
    // 敵の行動を開始。蓄力中なら突進を解放し、そうでなければ AI を実行する
    void EnemyStartAction();
    virtual void OnTurnChanged(TurnState state) override;
    virtual void TakeDamage(int damage, Unit* attacker) override;

    // ---------------------------------------------------------
    // ステータス・クエリ (Status Queries)
    // ---------------------------------------------------------
    void SetDisplayOrder(int order) { m_displayOrder = order; }
    bool IsIdle() const { return m_state == EnemyState::IDLE; }    
    bool IsDeadFlying() const {
        return m_state == EnemyState::DEAD_FLYING || m_state == EnemyState::DEATH_SHAKE;
    }
    bool IsCharging() const { return m_isCharging; }	
    Unit* GetLockedVictim() const { return m_lockedVictim; }
    int GetLockedGridX() const { return m_lockedVictim ? m_lockedVictim->GetUnitGridX() : -1; }
    int GetLockedGridZ() const { return m_lockedVictim ? m_lockedVictim->GetUnitGridZ() : -1; }
    int GetEnemyDamage() const { return m_enemyDamage; }
    // 初期向きをプレイヤー方向へ設定する（斜めの場合は水平方向を優先）
    void SetInitialFacingToPlayer();

    void ResetCharge() {
        m_isCharging = false;
        m_pendingCharge = false;
        m_lockedVictim = nullptr;
    }

    // ---------------------------------------------------------
    // UI レンダリング (UI Rendering)
    // ---------------------------------------------------------
    virtual void DrawUI() override;


    void PlayActionOrderBounce(int times);   // 行動順の数字を times 回バウンスさせる
    bool IsActionOrderBouncing() const;

protected:
    using Unit::Unit;

    virtual void OnDrawOverlay(float deltaSeconds) override;
    virtual void OnDrawFloorUI(float deltaSeconds) override;

    bool CanBePushed() const override { return m_currentHP > 0 && m_state != EnemyState::DEAD_FLYING; }
    void OnKnockbackBegin() override;
    void OnKnockbackEnd() override;

private:
    void Init();

    // ---------------------------------------------------------
    // 内部AI・ロジック (Internal AI Logic)
    // ---------------------------------------------------------
    
    // ターゲットを選定し、攻撃（チャージ開始）か接近移動かを判断する
    void ExecuteAI();
    // 指定ターゲットへの行動（攻撃 or 接近）を試みる。行動を開始できたら true。
    // 到達不可・近づけない場合は false を返し、呼び出し側が次の候補へフォールバックする
    bool TryActOnTarget(Unit* target);
    // 行動を終了し IDLE へ戻す
    void EnemyEndAction();
    // 移動完了時の着地処理。攻撃範囲内ならチャージへ移行する
    void OnMoveFinished();
    // ターゲットをロックオンし、突進前の蓄力（チャージ）に入る
    void StartCharge(Unit* target);
    // 蓄力を解放し、突進攻撃を発動する
    void ReleaseChargeAttack();
    // 死亡確定。チャージを解除し、吹き飛び演出とキルカメラへ移行する
    void Die();
    // 吹き飛び中の更新（基底の死亡飛翔処理へ委譲）
    void DeathFlyingUpdate(float delta);
    virtual void OnDeathFlyComplete() override;
    // ロック対象が命中する見込みがあるか（有効かつ攻撃範囲内） 
    bool WillHitLockedVictim() const;
    // 攻撃予警範囲（矩形）の中心と yaw を算出（ロック対象へ向く連続方向） 
    void GetAttackBox(Vector3& center, float& yaw) const;

    bool DriveAlongPath(float dt);                 // ウェイポイント追従（壁沿い移動＋回避＋移動予算）
    void BuildPathTo(Unit* target);                // BFS＋string-pulling で経路構築
    bool SegmentClear(const Vector3& a, const Vector3& b) const; // 線分の壁に対するLOS判定

    bool WouldHitVictim(Unit* v) const;     // v へ向いた矩形 ∩ v の被弾円
    void SyncGridFromWorld();               // 連続座標 → 論理グリッド m_gridX/Z（発動なし）


    // =========================================================
    // メンバー変数
    // =========================================================
    CShader* m_enemyShader = nullptr;
    std::unique_ptr<EnemyActionUI> m_actionUI;

    EnemyState m_state = EnemyState::IDLE;

    int m_enemyDamage = 2;
    int m_displayOrder = 0;
    float m_deathShakeTimer = 0.0f;

    // --- 連続座標での接近（移動予算＝距離） ---
    Unit* m_moveTarget = nullptr;// 接近対象
    std::vector<Vector3> m_pathWaypoints;   // 障害物回避用の中継点（ワールド座標）
    size_t m_waypointIndex = 0;


    // --- 攻撃・チャージ ---
    float m_attackTimer = 0.0f;
    Unit* m_lockedVictim = nullptr;   // ロック対象（連続座標で直接参照）
    bool m_isCharging = false;
    bool m_pendingCharge = false;
    Vector3 m_shakeOffset = Vector3(0, 0, 0); // 蓄力中の震え。m_srt.pos を汚さず描画時のみ加算する
};