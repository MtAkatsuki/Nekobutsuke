#include "ScreenToWorld.h"
#include "../../Core/Application.h"

Vector3 ScreenToWorld::GetNDC() const {
	// 画面サイズに基づきビューポート行列を構築
	Matrix4x4 mtxViewport = Matrix4x4::Identity;
	float halfWidth = Application::GetWidth() / 2.0f;
	float halfHeight = Application::GetHeight() / 2.0f;

	mtxViewport._11 = halfWidth;
	mtxViewport._22 = -halfHeight; // Y軸反転
	mtxViewport._41 = halfWidth;
	mtxViewport._42 = halfHeight;

	Vector3 screenPos(static_cast<float>(m_mouseX), static_cast<float>(m_mouseY), 0.0f);

	// ビューポート行列の逆行列を用いて、スクリーン座標からNDC（[-1, 1]空間）へ逆変換
	Matrix4x4 invViewport = mtxViewport.Invert();
	return screenPos.Transform(screenPos, invViewport);
}

Vector3 ScreenToWorld::GetViewCoordinate(float depth, const Matrix4x4& projmtx) const {
	Vector3 ndcPos = GetNDC();
	ndcPos.z = depth;

	// プロジェクション行列の逆行列を求め、NDCからビュー空間へ逆変換
	Matrix4x4 invProj = projmtx.Invert();
	Vector3 viewPos = ndcPos.Transform(ndcPos, invProj);

	// 逆透視除算（Inverse Perspective Division）
	// W成分を計算し、それで割ることで同次座標系からデカルト座標系に戻す
	float w = (ndcPos.x * invProj._14) +
		(ndcPos.y * invProj._24) +
		(ndcPos.z * invProj._34) +
		invProj._44;

	viewPos.x /= w;
	viewPos.y /= w;
	viewPos.z /= w;

	return viewPos;
}

Vector3 ScreenToWorld::GetWorldCoordinate(float depth, const Matrix4x4& projmtx, const Matrix4x4& viewmtx) const {
	Vector3 clipPos = GetViewCoordinate(depth, projmtx);

	// ビュー行列の逆行列を求め、ビュー空間からワールド空間へ逆変換
	Matrix4x4 invView = viewmtx.Invert();
	Vector3 worldPos = clipPos.Transform(clipPos, invView);

	// 同次除算
	float w = (worldPos.x * invView._14) +
		(worldPos.y * invView._24) +
		(worldPos.z * invView._34) +
		invView._44;

	worldPos.x /= w;
	worldPos.y /= w;
	worldPos.z /= w;

	return worldPos;
}