#include "WorldToScreen.h"

Vector2 WorldToScreen(
    const Vector3& worldPos,
    const Matrix4x4& view,
    const Matrix4x4& proj,
    int screenWidth,
    int screenHeight)
{
    // 同次座標（Homogeneous Coordinates）への拡張 (w=1.0)
    Vector4 worldPosH(worldPos.x, worldPos.y, worldPos.z, 1.0f);

    // 1. クリップ空間（Clip Space）への変換
    // ワールド -> ビュー -> プロジェクションの順に行列を乗算
    Vector4 clipPos = Vector4::Transform(worldPosH, view);
    clipPos = Vector4::Transform(clipPos, proj);

    // カメラの背後判定
    // w値が0以下の場合はカメラの後方にあるため、無効な座標として扱う
    if (clipPos.w <= 0.0f) {
        return Vector2(-1.0f, -1.0f);
    }

    // 2. 透視除算（Perspective Division） -> 正規化デバイス座標 (NDC)
    // 同次座標の w 成分で割ることで、遠近感（パース）を適用し、[-1, 1] の空間に変換する
    float ndcX = clipPos.x / clipPos.w;
    float ndcY = clipPos.y / clipPos.w;

    // 3. スクリーン座標へのマッピング
    // NDC [-1, 1] を スクリーンピクセル [0, Width/Height] に変換（Y軸は反転）
    float screenX = (ndcX + 1.0f) * 0.5f * screenWidth;
    float screenY = (1.0f - (ndcY + 1.0f) * 0.5f) * screenHeight;

    return Vector2(screenX, screenY);
}