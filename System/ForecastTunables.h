#pragma once
#include "CommonTypes.h"   // Color（= DirectX::SimpleMath::Color）

// ノックバック予測（AIM）の見た目に関するパラメータを一括管理。
// 被弾円（敵の足元）と着地点の円で、「半径」「ボーダーの太さ」を個別に設定する。
namespace ForecastUI {
    // --- 被弾円（敵の足元・敵／罠に踏まれる＝床レイヤー） ---
    inline float HitRingRadius = 0.6f;                            // 判定と描画で共用（値を一致させる）
    inline float HitRingBorder = 0.05f;                          // 被弾円のボーダーの太さ（個別設定）
    inline Color HitRingColor = Color(0.35f, 0.6f, 1.0f, 1.0f); // 青

    // --- 着地点の円 ---
    inline float LandingRingRadius = 0.45f;
    inline float LandingRingBorder = 0.08f;                      // 着地点の円のボーダーの太さ（個別設定）

    // --- 透明度 ---
    inline float RingFillAlpha = 0.35f;   // 円の塗り
    inline float RingBorderAlpha = 0.7f;    // 円のボーダー
    inline float ArrowAlpha = 0.6f;    // 放物線状の矢印
}
