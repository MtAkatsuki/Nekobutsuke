#pragma once
#include "Unit.h"
#include "../system/CStaticMesh.h"
#include "../system/CStaticMeshRenderer.h"

class Ally : public Unit {
public:
    using Unit::Unit;

    void init() override {} // Unitのバーチャル関数
    void Init(); // 実際の初期化
    void dispose() override {}

    void Update(uint64_t delta) override;
    void OnDraw(uint64_t delta) override;//Allyだけの描画

    //Allyはプッシュからの位置変化を受けない
    void TakeDamage(int damage, Unit* attacker)override;

    virtual void StartTurn() override;
    void OnTurnChanged(TurnState state) override;
	// 脱出関連のの関数
    void TriggerEscape();
	// 脱出アニメーションが完了したかどうかを判定する関数
    bool IsEscapeDone() const { return m_escapeState == EscapeState::Done; }

    // 仲間の脱出アニメーション（掘削中、またはフェードアウト中）の状態を判定
    bool IsEscaping() const {
        return m_escapeState == EscapeState::Digging || m_escapeState == EscapeState::Fading;
    }
private:
    CShader* m_shader = nullptr;

    void UpdateDiggingAnimation(float dt);

    // 採掘（マイニング）関連の変数
    bool m_isDigging = false;        // 採掘中フラグ
    float m_digTimer = 0.0f;         // 採掘用タイマー
    int m_digCount = 0;              // 現在の採掘回数（何回振ったか）
    const int MAX_DIG_COUNT = 3;     // 最大採掘回数
    const float DIG_SPEED = 15.0f;   // 採掘アニメーションの速度
    bool m_hasTriggeredEffect = false; // 現在の振り動作でエフェクトを発生済みかどうかの判定用
	// 脱出関連の変数
    bool m_isEscaping = false;
    float m_escapeAlpha = 1.0f;
	// 脱出の状態管理用の列挙型
    enum class EscapeState {
        None,
        Digging,
        Fading,
        Done
    };
    EscapeState m_escapeState = EscapeState::None;
};