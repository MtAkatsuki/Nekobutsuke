#pragma once
#include "CommonTypes.h"   // Color（= DirectX::SimpleMath::Color）

// ノックバック予測（AIM）の見た目に関するパラメータを一括管理。
// 被弾円（敵の足元）と着地点の円で、「半径」「ボーダーの太さ」を個別に設定する。
namespace ForecastUI {
    // --- 被弾円（敵の足元・敵／罠に踏まれる＝床レイヤー） ---
    inline float HitRingRadius = 0.48f;                            // 判定と描画で共用（値を一致させる）
    inline Color HitRingColor = Color(0.35f, 0.6f, 1.0f, 1.0f); // 青

    // --- 着地点の円 ---
    inline float LandingRingRadius = 0.48f;

    // --- 透明度 ---
    inline float RingFillAlpha = 1.00f;   // 円の塗り
    inline float RingBorderAlpha = 1.00f;    // 円のボーダー
    inline float ArrowAlpha = 0.42f;    // 放物線状の矢印
}
