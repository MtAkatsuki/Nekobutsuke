#pragma once
#include "../system/commontypes.h"

// =========================================================
// ScreenToWorld クラス
// マウスクリックなどの2Dスクリーン座標から、逆変換計算を行って
// 3D空間のワールド座標（レイの始点など）を算出する。
// =========================================================
class ScreenToWorld {
public:
    ScreenToWorld() = delete;
    ScreenToWorld(int mouseX, int mouseY)
        : m_mouseX(mouseX), m_mouseY(mouseY) {
    }
    ~ScreenToWorld() = default;

    // 正規化デバイス座標 (NDC) の取得
    Vector3 GetNDC() const;

    // ビュー空間の座標を取得 (depth: 求めたい深度Z)
    Vector3 GetViewCoordinate(float depth, const Matrix4x4& projmtx) const;

    // ワールド空間の座標を取得
    Vector3 GetWorldCoordinate(float depth, const Matrix4x4& projmtx, const Matrix4x4& viewmtx) const;

private:
    int m_mouseX = 0;
    int m_mouseY = 0;
};