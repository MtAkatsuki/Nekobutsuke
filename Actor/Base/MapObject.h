#pragma once

#include "../../Actor/Base/GameObject.h"
#include "../../System/CStaticMeshRenderer.h"
#include "../../Types/MapModelType.h"

class Unit;

// =========================================================
// MapObject クラス
// ステージを構成するすべての静的・動的オブジェクトの基底クラス
// =========================================================
class MapObject : public GameObject {
public:
	explicit MapObject(GameContext* context);
	virtual ~MapObject() {}

	// ---------------------------------------------------------
	// ライフサイクル (Lifecycle)
	// ---------------------------------------------------------
	void Dispose() override {}
	virtual void Init(MapModelType type, Vector3 position);
	virtual void Update(float deltaSeconds) override;
	void OnDraw(float deltaSeconds) override;

	// ---------------------------------------------------------
	// 属性・判定インターフェース (Attributes & Checks)
	// ---------------------------------------------------------
	// オブジェクト上を通行可能かどうか
	virtual bool IsWalkable() const { return m_isWalkable; }

	// ユニットがこのオブジェクトのマスに進入した際のコールバック（Trap等でオーバーライド）
	virtual void OnEnter(Unit* unit) {}

	// ---------------------------------------------------------
	// ゲッター / セッター (Getters & Setters)
	// ---------------------------------------------------------
	MapModelType GetType() const { return m_type; }
	MapObject* AsMapObject() override { return this; }

	void SetGridPosition(int x, int z) {
		m_gridX = x;
		m_gridZ = z;
	}
	int GetGridX() const { return m_gridX; }
	int GetGridZ() const { return m_gridZ; }

protected:
	CStaticMeshRenderer* m_renderer = nullptr;
	MapModelType m_type = MapModelType::FLOOR;

	bool m_isWalkable = true;

	// --- 共通アルファ（透明度）管理 ---
	// プロップの遮蔽時や、トラップの消失アニメーションで共有利用する
	float m_currentAlpha = 1.0f;
	float m_targetAlpha = 1.0f;

	// グリッド座標
	int m_gridX = 0;
	int m_gridZ = 0;
};