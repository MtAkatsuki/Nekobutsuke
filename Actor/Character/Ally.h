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
    void DrawUI() override;

    // ---------------------------------------------------------
    // フロー制御・イベント (Flow & Events)
    // ---------------------------------------------------------
    void TakeDamage(int damage, Unit* attacker) override;
    virtual void OnDeathFlyComplete() override;
    virtual void StartTurn() override;
    void OnTurnChanged(TurnState state) override;
    virtual void OnPushed(Direction pushDir, Unit* attacker = nullptr) override;

    // 脱出を予約する（規定ターン到達時に GameScene から呼ぶ）。
    // 実際の演出は次のプレイヤーフェーズの採掘完了を起点に進行する
    void ArmEscape();
    bool IsEscapeDone() const { return m_escapeState == EscapeState::Done; }
    // 脱出シーケンス進行中か（Armed～Fading）。ジャンプ警告・被押し出し・被ダメージの抑止に使う
    bool IsEscaping() const {
        return m_escapeState != EscapeState::None && m_escapeState != EscapeState::Done;
    }
    // 脱出点（マーカー/キューブ）を表示すべきか（採掘完了以降）
    bool IsEscapePointVisible() const {
        return m_escapeState == EscapeState::PointReveal
            || m_escapeState == EscapeState::Speaking
            || m_escapeState == EscapeState::Fading
            || m_escapeState == EscapeState::Done;
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
        Armed,       // 脱出待機中（次の採掘で脱出地点を表示）
        PointReveal, // 脱出地点を表示し、台詞表示まで待機
        Speaking,    // 脱出台詞を表示中
        Fading,      // フェードアウト中
        Done
    };
    EscapeState m_escapeState = EscapeState::None;
    float m_escapeSeqTimer = 0.0f;   // PointReveal/Speaking の経過時間
    float m_escapeAlpha = 1.0f;
    bool m_isKnockedBack = false;
    bool m_isDeadFlying = false;

    // --- 危険警告（ロックオンされる時のジャンプ） ---
    bool  m_isAlarmed = false;
    float m_alarmTimer = 0.0f;
    float m_jumpBaseY = 0.0f;
    bool  m_alarmSignalStarted = false;  // 警告演出（ジャンプ＋吹き出し）を開始済みか
};