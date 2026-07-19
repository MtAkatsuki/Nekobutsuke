#pragma once
#include "../Base/Unit.h"
#include "../../System/CStaticMesh.h"
#include "../../System/CStaticMeshRenderer.h"

// =========================================================
// Ally クラス
// プレイヤーの護衛対象となる味方ユニット（ネズミ）。
// 独自の脱出（採掘）アニメーションと無敵状態の制御を持つ。
// =========================================================
class Ally : public Unit {
public:

    // ---------------------------------------------------------
    // ライフサイクル (Lifecycle)
    // ---------------------------------------------------------
    static std::unique_ptr<Ally> Spawn(GameContext* ctx, int gridX, int gridZ, const Vector3& worldPos);
    void Dispose() override {}
    void Update(float deltaSeconds) override;

    // ---------------------------------------------------------
    // レンダリング (Rendering)
    // ---------------------------------------------------------
    void OnDraw(float deltaSeconds) override;

    // ---------------------------------------------------------
    // フロー制御・イベント (Flow & Events)
    // ---------------------------------------------------------
    void TakeDamage(int damage, Unit* attacker) override;
    virtual void OnDeathFlyComplete() override;
    virtual void StartTurn() override;
    void OnTurnChanged(TurnState state) override;
    virtual void OnPushed(Direction pushDir, Unit* attacker = nullptr) override;

    // 脱出を開始。無敵化し、採掘 → フェードアウトの演出へ移行する
    void TriggerEscape();
    bool IsEscapeDone() const { return m_escapeState == EscapeState::Done; }
    bool IsEscaping() const {
        return m_escapeState == EscapeState::Digging || m_escapeState == EscapeState::Fading;
    }

protected:
    using Unit::Unit;

private:
    void Init();
    // 採掘（ツルハシ振り）アニメーションを更新する
    void UpdateDiggingAnimation(float dt);
    // 危険警告（敵にロックオンされた際のジャンプ）の状態を更新する
    void UpdateAlarmState(float dt);

    // =========================================================
    // メンバー変数
    // =========================================================
    CShader* m_shader = nullptr;
    // --- 識別用アウトラインの呼吸パルス ---
    float m_outlinePulseTimer = 0.0f;

    // --- 採掘アニメーション ---
    bool m_isDigging = false;
    float m_digTimer = 0.0f;
    int m_digCount = 0;
    bool m_hasTriggeredEffect = false;
    int m_digTargetCount = 3;          // 今回の採掘で振る回数（最初の一回のみ多め）
    bool m_hasPlayedIntroDig = false;  // 最初の一回を再生済みか

    // --- 脱出システム ---
    enum class EscapeState {
        None,
        Digging,
        Fading,
        Done
    };
    EscapeState m_escapeState = EscapeState::None;
    bool m_isEscaping = false;
    float m_escapeAlpha = 1.0f;
    bool m_isKnockedBack = false;
    bool m_isDeadFlying = false;

    // --- 危険警告（ロックオンされる時のジャンプ） ---
    bool  m_isAlarmed = false;
    float m_alarmTimer = 0.0f;
    float m_jumpBaseY = 0.0f;
    bool  m_alarmSignalStarted = false;  // 警告演出（ジャンプ＋吹き出し）を開始済みか
};