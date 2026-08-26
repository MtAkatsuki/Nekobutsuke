#include	"../../Actor/Base/MapObject.h"
#include    "../../System/MeshManager.h"
#include "../../System/CMaterial.h"
#include <cmath>

MapObject::MapObject(GameContext* context):GameObject(context){}

void MapObject::Init(MapModelType type, Vector3 position) {
	m_srt.pos = position;
	m_type = type;

	// 通行可能属性はプロップ定義テーブルから取得（未登録タイプ＝床などは通行可能）
	const PropDef* def = FindPropDef(type);
	m_isWalkable = def ? def->walkable : true;

	if (type == MapModelType::FLOOR) {
		m_renderer = MeshManager::GetRenderer<CStaticMeshRenderer>("floor_mesh");
	}

	m_srt.scale = Vector3(1.0f, 1.0f, 1.0f);
	UpdateWorldMatrix();
}

void MapObject::Update(float /*deltaSeconds*/) {
    // ベースクラスのUpdateは空実装（派生クラスで必要に応じて拡張）
}

void MapObject::OnDraw(float deltaSeconds)
{
    // 先に fade を更新（完全に非表示でも更新を続け、遮蔽解除時に復帰できるようにする）
    UpdateFade(deltaSeconds);

    if (m_renderer == nullptr) return;
    if (m_fade >= 0.999f) return;              // 完全に非表示：描画をスキップ

    const bool fading = (m_fade > 0.001f);
    if (fading) ApplyFadeToMaterials(m_fade);
    m_renderer->Draw();
    if (fading) ApplyFadeToMaterials(0.0f);
}

void MapObject::UpdateFade(float dt) {
    // フレームレートに依存せず、目標 fade 値へ滑らかに遷移
    if (m_fade == m_targetFade) return;
    float t = 1.0f - expf(-FADE_LERP_SPEED * dt);
    m_fade += (m_targetFade - m_fade) * t;
    if (fabsf(m_fade - m_targetFade) < 0.001f) m_fade = m_targetFade;
}

void MapObject::ApplyFadeToMaterials(float fade) {
    // すべての子マテリアルの Dummy.x に fade 値を設定
    if (!m_renderer) return;
    for (int i = 0; CMaterial * mtrl = m_renderer->GetMaterial(i); ++i) {
        MATERIAL data = mtrl->GetData();
        data.Dummy[0] = fade;
        mtrl->SetMaterial(data);
    }
}

