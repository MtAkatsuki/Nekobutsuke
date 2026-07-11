#pragma once

#include "../Base/MapObject.h"
#include "../../System/CStaticMesh.h"

// =========================================================
// Prop クラス
// 家具や障害物などの環境モデル。
// プレイヤーや敵が裏側に隠れた際、視認性を確保するための半透明化（Occlusion）機能を持つ。
// =========================================================
class Prop : public MapObject {
public:
	using MapObject::MapObject;

	// ---------------------------------------------------------
	// ライフサイクル (Lifecycle)
	// ---------------------------------------------------------
	void Init(MapModelType type, Vector3 position) override;
	void Update(uint64_t delta) override;

	// ---------------------------------------------------------
	// レンダリング (Rendering)
	// ---------------------------------------------------------
	void OnDraw(uint64_t delta) override;

	// 家具の占有サイズを取得（MapManagerでのタイル埋め立て用）
	
	// outW: X軸方向の占有マス数, outD: Z軸方向の占有マス数
	static void GetDimensions(MapModelType type, int& outW, int& outD);

private:
	int m_sizeX = 1; // 占有する幅（X軸）
	int m_sizeZ = 1; // 占有する奥行き（Z軸）
private:
	void DrawPropShadow();
};