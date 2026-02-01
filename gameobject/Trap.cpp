#include "Trap.h"
#include "../system/meshmanager.h"
#include "../gameobject/Unit.h"
#include <iostream>

void Trap::Init(MapModelType type, Vector3 position)
{
    // 基底クラスの初期化メソッドを呼び出し
    MapObject::Init(type, position);

    m_renderer = MeshManager::getRenderer<CStaticMeshRenderer>("floor_mesh");

    // 地面との重なり（Z-Fighting）を防ぐため、Y座標をわずかに上げる
    m_srt.pos.y = 0.5f;
    m_srt.rot.x = 0.0f;  // 平面に配置
    m_srt.scale = Vector3(1.0f, 1.0f, 1.0f); // 判定に合わせて少し小さめに設定

    UpdateWorldMatrix();
}

void Trap::OnEnter(Unit* unit)
{
    // 既に発動済みの場合は処理をスキップ
    if (m_isActivated) return;

    if (unit) {
        // デバッグ出力：トラップ発動
        std::cout << "TRAP ACTIVATED! Unit took damage." << std::endl;

        // ユニットに2ポイントのダメージを与える（攻撃者は無し）
        unit->TakeDamage(6, nullptr);

        // 発動済みフラグを立てる
        m_isActivated = true;
    }
}

void Trap::OnDraw(uint64_t delta)
{
    // レンダラーが有効でない場合は描画しない
    if (!m_renderer) return;

    Renderer::SetWorldMatrix(&m_WorldMatrix);
    m_renderer->Draw();
}