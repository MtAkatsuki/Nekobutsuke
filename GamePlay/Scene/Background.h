#pragma once
#include "../../System/CSprite.h"
#include <memory>

// =========================================================
// Background クラス
// シーンの背景（ストライプ等）の描画と、UVスクロールによる
// 無限スクロールアニメーションの制御を担当する。
// =========================================================
class Background {
public:
    Background() = default;
    ~Background() = default;

    // ---------------------------------------------------------
    // ライフサイクル (Lifecycle)
    // ---------------------------------------------------------
    void Init(const std::string& texturePath = "Assets/texture/bg_stripe.png");
    void Update(float deltaSeconds);

    // ---------------------------------------------------------
    // レンダリング (Rendering)
    // ---------------------------------------------------------
    void Draw();

    // ---------------------------------------------------------
    // ゲッター / セッター
    // ---------------------------------------------------------
    void SetAlpha(float alpha) { m_alpha = alpha; }
    void SetScrollSpeed(float speed) { m_scrollSpeed = speed; }

private:
    std::unique_ptr<CSprite> m_sprite;

    // --- 状態管理 ---
    float m_scrollOffset = 0.0f;
    float m_alpha = 1.0f;

    // --- アニメーション・描画パラメータ ---
    float m_scrollSpeed = 0.3f;  // スクロール速度
    static constexpr float TILE_REPEAT = 10.0f; // テクスチャの繰り返し回数
};