#include	"../gameobject/MapObject.h"
#include    "../system/meshmanager.h"

MapObject::MapObject(GameContext* context):GameObject(context){}

void MapObject::Init(MapModelType type, Vector3 position)
{
	m_srt.pos = position;

	if (type == MapModelType::FLOOR) 
	{
		//grassÇÃrendererçÏê¨
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



