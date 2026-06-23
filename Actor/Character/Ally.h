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
    using Unit::Unit;

    // ---------------------------------------------------------
    // ライフサイクル (Lifecycle)
    // ---------------------------------------------------------
    void init() override {}
    void Init();
    void dispose() override {}
    void Update(uint64_t delta) override;

    // ---------------------------------------------------------
    // レンダリング (Rendering)
    // ---------------------------------------------------------
    void OnDraw(uint64_t delta) override;

    // ---------------------------------------------------------
    // フロー制御・イベント (Flow & Events)
    // ---------------------------------------------------------
    void TakeDamage(int damage, Unit* attacker) override;
    virtual void StartTurn() override;
    void OnTurnChanged(TurnState state) override;
    virtual void OnPushed(Direction pushDir, Unit* attacker = nullptr) override;

    void TriggerEscape();
    bool IsEscapeDone() const { return m_escapeState == EscapeState::Done; }
    bool IsEscaping() const {
        return m_escapeState == EscapeState::Digging || m_escapeState == EscapeState::Fading;
    }

private:
    void UpdateDiggingAnimation(float dt);

    // =========================================================
    // メンバー変数
    // =========================================================
    CShader* m_shader = nullptr;

    // --- 採掘アニメーション ---
    bool m_isDigging = false;
    float m_digTimer = 0.0f;
    int m_digCount = 0;
    bool m_hasTriggeredEffect = false;

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
};