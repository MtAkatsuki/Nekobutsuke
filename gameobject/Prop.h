#pragma once
#include "MapObject.h"

class Prop : public MapObject
{
public:
	using MapObject::MapObject; // 親クラスのコンストラクタを継承

	// Prop（障害物）は通行不可
    bool IsWalkable() const override { return false; }

    void Init(MapModelType type, Vector3 position) override;
    void OnDraw(uint64_t delta) override;
};