#pragma once
#include	"../gameobject/GameObject.h"
#include	"../system/CStaticMeshRenderer.h"

class Unit;

enum class MapModelType {
	FLOOR,            // 床
	WALL,             // 壁
	TRAP,             // トラップ

	// --- 家具・プロップ (長方形・大型物件) ---
	PROP_SOFA_YOKO,      // ソファ（横向き 3x1）
	PROP_SOFA_TATE,      // ソファ（縦向き 1x3） [新規追加]
	PROP_BOOKSHELF,   // 本棚（横 2x1, 高さ 2.5）
	PROP_CATTOWER,    // キャットタワー（横 2x1, 高さ 3.0）
	PROP_TABLE        // テーブル（正方形 2x2）
};

class MapObject : public GameObject
{
public:
	MapObject(GameContext* context);
	virtual ~MapObject() {}

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

	//  基準となるグリッド座標の設定
	void SetGridPosition(int x, int z) {
		m_gridX = x;
		m_gridZ = z;
	}
	// グリッド座標の取得
	int GetGridX() const { return m_gridX; }
	int GetGridZ() const { return m_gridZ; }


protected:
	CStaticMeshRenderer* m_renderer;
	MapModelType m_type = MapModelType::FLOOR;
	bool m_isWalkable = true;

	// 透明度制御用変数
	float m_currentAlpha = 1.0f; // 現在の透明度
	float m_targetAlpha = 1.0f;  // 目標とする透明度

	// グリッド座標
	int m_gridX = 0;
	int m_gridZ = 0;
};