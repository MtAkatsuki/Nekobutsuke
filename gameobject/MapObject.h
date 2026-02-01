#pragma once
#include	"../gameobject/GameObject.h"
#include	"../system/CStaticMeshRenderer.h"

class Unit;

enum class MapModelType {
	FLOOR,
	WALL,
	TRAP,
	PROP_SOFA_YELLOW, // W_Y_SOFA
	PROP_SOFA_WHITE,  // W_W_SOFA
	PROP_CATTOWER,    // W_CATTOWER
	PROP_TABLE,       // W_TABLE
	PROP_BOOKSHELF    // W_BOOKSHELF
};

class MapObject : public GameObject
{
public:
	MapObject(GameContext* context);
	~MapObject() {}

	void init() override {}
	void dispose() override {}

	virtual void Init(MapModelType type, Vector3 position);

	virtual void Update(uint64_t delta);
	void OnDraw(uint64_t delta);
	//マップのオブジェクトは通るかどうか
	virtual bool IsWalkable() const { return true; }
	//トラプトホールがユニットに入ったときの処理
	virtual void OnEnter(Unit* unit) {}
	//オブジェクトの種類を取得(罠判断用)
	MapModelType GetType() const { return m_type; }

protected:
	CStaticMeshRenderer* m_renderer;
	MapModelType m_type = MapModelType::FLOOR;
	bool m_isWalkable = true;
};