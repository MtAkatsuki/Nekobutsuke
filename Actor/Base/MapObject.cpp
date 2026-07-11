#include	"../../Actor/Base/MapObject.h"
#include    "../../System/MeshManager.h"

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

void MapObject::Update(uint64_t delta) {
    // ベースクラスのUpdateは空実装（派生クラスで必要に応じて拡張）
}

void MapObject::OnDraw(uint64_t delta)
{
	if (m_renderer != nullptr) {
		m_renderer->Draw();
	}
}



