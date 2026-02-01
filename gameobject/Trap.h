#pragma once
#include "MapObject.h"

class Trap : public MapObject
{
public:
    using MapObject::MapObject;

	// Trap（罠）は通行可能
    bool IsWalkable() const override { return true; }

    void Init(MapModelType type, Vector3 position) override;

	// ユニットが罠に入ったときの処理
    void OnEnter(Unit* unit) override;
    void OnDraw(uint64_t delta) override;

private:
	bool m_isActivated = false; // 罠が発動したかどうかのフラグ
};