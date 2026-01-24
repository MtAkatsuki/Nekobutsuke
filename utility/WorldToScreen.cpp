#include"WorldToScreen.h"

//入力：3Dワールド座標、カメラビュー行列、投影行列、スクリーンサイズ
//出力：2Dスクリーンピクセル座標

Vector2 WorldToScreen(
	const Vector3& worldPos,
	const Matrix4x4& view,//ビュー空間
	const Matrix4x4& proj,//
	int screenWidth,
	int screenHeight
)

{	// 同次座標（Homogeneous Coordinates）
	Vector4 worldPosH(worldPos.x, worldPos.y, worldPos.z, 1.0f);//点：w=1、方向：w=0

	// クリップ空間へ変換
	Vector4 clipPos = Vector4::Transform(worldPosH, view);//ビュー変換行列でworld->view
	clipPos = Vector4::Transform(clipPos, proj);//プロジェクション変換行列で(world->view)->clip
	/*
	 クリップ空間説明
	 1.クリッピング空間の特性：正規化された四次元空間 (x, y, z, w) である。
     2.この時点ではまだ透視除算（Perspective Division）は行われておらず、直接的にスクリーン位置とし解釈することはできない。
	 3.GPUのクリッピングステージでは、この座標を用いて視錐台内（-w ≤ x,y,z ≤ w）にあるかどうかを判断する。
	*/

	// 同次除算 → NDC
	if (clipPos.w <= 0.0f) 
	{
		return Vector2(-1, -1);//カメラの後ろにあり、無効
	}
	//透視除算でクリッピング空間をNDC（Normalized Device Coordinates、正規化デバイス座標）へ変換
	//NDCの範囲：x, y, z ∈ [-1, 1]（DirectXのデフォルト。Zの範囲は異なる場合があるが、X/Yは統一されている）。
	float ndcX = clipPos.x / clipPos.w;
	float ndcY = clipPos.y / clipPos.w;

	//NDC [-1,1] → スクリーン [0, width/height] へ変換（Y軸の反転に注意）
	float screenX = (ndcX + 1.0f) * 0.5f * screenWidth;
	float screenY = (1.0 - (ndcY + 1.0f) * 0.5f) * screenHeight;

	return Vector2(screenX, screenY);
}