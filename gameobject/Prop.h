#pragma once
#include "MapObject.h"
#include "../system/CStaticMesh.h"
class Prop : public MapObject
{
public:
	using MapObject::MapObject; // 親クラスのコンストラクタを継承

	// Prop（障害物）は通行不可
    bool IsWalkable() const override { return false; }

    void Init(MapModelType type, Vector3 position) override;
    void OnDraw(uint64_t delta) override;
    // 家具の占有サイズを取得（MapManagerでのタイル埋め立て用）
    // outW: X軸方向の占有マス数, outD: Z軸方向の占有マス数
    static void GetDimensions(MapModelType type, int& outW, int& outD);

private:

    std::unique_ptr<CTexture> m_texture;
    int m_sizeX = 1; // 占有する幅（X軸）
    int m_sizeZ = 1; // 占有する奥行き（Z軸）

};