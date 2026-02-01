#include "Prop.h"
#include "../system/meshmanager.h"
#include "../manager/GameContext.h"

void Prop::Init(MapModelType type, Vector3 position)
{
    // 基底クラスのメソッドを呼び出し、座標を初期化
    MapObject::Init(type, position);

    switch (type) {
    case MapModelType::WALL:
        m_renderer = MeshManager::getRenderer<CStaticMeshRenderer>("floor_mesh");
        break;
    case MapModelType::PROP_SOFA_YELLOW:
        m_renderer = MeshManager::getRenderer<CStaticMeshRenderer>("floor_mesh");
        break;
    case MapModelType::PROP_SOFA_WHITE:
        m_renderer = MeshManager::getRenderer<CStaticMeshRenderer>("floor_mesh");
        break;
    case MapModelType::PROP_CATTOWER:
        m_renderer = MeshManager::getRenderer<CStaticMeshRenderer>("floor_mesh");
        break;
        
    }

    if (!m_renderer) {
        m_renderer = MeshManager::getRenderer<CStaticMeshRenderer>("floor_mesh");
    }

    // 斜めビルボード (Tilted Billboard) の回転設定
    // X軸を約 -45度 (-0.785ラジアン) 回転させ、カメラの方へ向くように立たせる
    m_srt.rot.x = -0.785f;

    // 地面とのZファイティング（描画のチラつき）を避けるため、Y座標をわずかに底上げ
    m_srt.pos.y = 0.5f;

    // スケールの設定
    m_srt.scale = Vector3(1.0f, 1.0f, 1.0f);

    // ワールド行列を更新
    UpdateWorldMatrix();
}

void Prop::OnDraw(uint64_t delta)
{
    // レンダラーが未設定の場合は描画をスキップ
    if (!m_renderer) return;

    Renderer::SetWorldMatrix(&m_WorldMatrix);
    m_renderer->Draw();
}