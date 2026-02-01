#include	"../gameobject/MapObject.h"
#include    "../system/meshmanager.h"

MapObject::MapObject(GameContext* context):GameObject(context){}

void MapObject::Init(MapModelType type, Vector3 position)
{
	m_srt.pos = position;
    // オブジェクトタイプを保存（AIの識別やロジック判定に使用）
    m_type = type;

    // 通行可能属性（m_isWalkable）の設定
    // デフォルトは通行可能だが、障害物（壁や家具）については false に設定する。
    // ※基底クラスで一括設定しているが、特殊な Prop の場合は各 Init でのオーバーライドも可能。
    switch (type) {
    case MapModelType::WALL:
    case MapModelType::PROP_SOFA_YELLOW:
    case MapModelType::PROP_SOFA_WHITE:
    case MapModelType::PROP_CATTOWER:
    case MapModelType::PROP_TABLE:
    case MapModelType::PROP_BOOKSHELF:
        // 障害物：通行不可能に設定
        m_isWalkable = false;
        break;

    case MapModelType::TRAP:
        // トラップ：通行は可能（ただしダメージ判定あり）なため true に設定
        m_isWalkable = true;
        break;

    default:
        // 床などはデフォルトで通行可能
        m_isWalkable = true;
        break;
    }
	if (type == MapModelType::FLOOR) 
	{
		//grassのrenderer作成
		m_renderer = MeshManager::getRenderer<CStaticMeshRenderer>("floor_mesh");
	}

	m_srt.scale = Vector3(1.0f, 1.0f, 1.0f);
	UpdateWorldMatrix();
}
void MapObject::Update(uint64_t delta) 
{
}
void MapObject::OnDraw(uint64_t delta)
{
	if (m_renderer != nullptr) {
		m_renderer->Draw();
	}
}



