#pragma once

#include "../Base/MapObject.h"
#include "../../System/CStaticMesh.h"

class CShader;

// =========================================================
// Prop クラス
// 家具や障害物などの静的な環境モデル。複数マスを占有し、接地影（Blob）を落とす。
// =========================================================
class Prop : public MapObject {
public:
	using MapObject::MapObject;

	// ---------------------------------------------------------
	// ライフサイクル (Lifecycle)
	// ---------------------------------------------------------
	// メッシュと占有サイズをプロップ定義テーブルから解決して配置する
	void Init(MapModelType type, Vector3 position) override;
	// 静的オブジェクトのため毎フレームの更新処理は持たない
	void Update(float deltaSeconds) override;

	// ---------------------------------------------------------
	// レンダリング (Rendering)
	// ---------------------------------------------------------
	// 接地影を描いてから本体をトゥーン描画する
	void OnDraw(float deltaSeconds) override;

private:
	int m_sizeX = 1; // 占有する幅（X軸）
	int m_sizeZ = 1; // 占有する奥行き（Z軸）

	// --- 描画リソースキャッシュ（毎フレームの文字列検索を回避） ---
	CShader* m_toonShader = nullptr;
	CShader* m_blobShader = nullptr;
	CStaticMeshRenderer* m_blobMesh = nullptr;
private:
	// 占有範囲に合わせた接地影（Blob）を描画する
	void DrawPropShadow();
};