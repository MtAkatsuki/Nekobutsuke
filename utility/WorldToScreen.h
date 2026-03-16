#pragma once
#include "../system/commontypes.h"

// =========================================================
// WorldToScreen
// 3D空間のワールド座標を、2Dのスクリーンピクセル座標に変換する。
// UIの追従表示（HPバーやダメージ数値など）に使用される。
// =========================================================
Vector2 WorldToScreen(
    const Vector3& worldPos,
    const Matrix4x4& view,
    const Matrix4x4& proj,
    int screenWidth,
    int screenHeight
);