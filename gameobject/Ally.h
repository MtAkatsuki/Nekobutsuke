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
};