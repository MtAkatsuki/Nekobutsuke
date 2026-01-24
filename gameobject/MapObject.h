#pragma once
#include	"../gameobject/GameObject.h"
#include	"../system/CStaticMeshRenderer.h"

enum class MapModelType {
	FLOOR,
	WALL,
	WATER,
	TRAP_HOLE,
	MECHANISM_OFF,
	MECHANISM_ON,
	BLANK,
	BLOCK
};

class MapObject : public GameObject
{
public:
	MapObject(GameContext* context);
	~MapObject() {}
	void init() override {}
	void dispose() override {}

	void Init(MapModelType type, Vector3 position);
	virtual void Update(uint64_t delta);
	void OnDraw(uint64_t delta);
private:
	//grass‚Æwall‚Ìrenderer
	CStaticMeshRenderer* m_renderer;
};