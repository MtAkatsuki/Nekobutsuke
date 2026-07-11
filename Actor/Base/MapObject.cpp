#include	"../../Actor/Base/MapObject.h"
#include    "../../System/meshmanager.h"

MapObject::MapObject(GameContext* context):GameObject(context){}

void MapObject::Init(MapModelType type, Vector3 position) {
	m_srt.pos = position;
	m_type = type;

	// オブジェクトタイプに基づく通行可能属性（Walkable）の初期設定
	switch (type) {
	case MapModelType::WALL:
	case MapModelType::PROP_SOFA_TATE:
	case MapModelType::PROP_SOFA_YOKO:
	case MapModelType::PROP_CATTOWER:
	case MapModelType::PROP_TABLE:
	case MapModelType::PROP_BOOKSHELF:
		m_isWalkable = false; // 障害物は通行不可
		break;

	case MapModelType::TRAP:
		m_isWalkable = true;  // トラップは進入可能なため true に設定（ダメージはOnEnterで処理）
		break;

	default:
		m_isWalkable = true;  // 床などはデフォルトで通行可能
		break;
	}

	if (type == MapModelType::FLOOR) {
		m_renderer = MeshManager::getRenderer<CStaticMeshRenderer>("floor_mesh");
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



