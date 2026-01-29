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

private:
    CShader* m_shader = nullptr;

};