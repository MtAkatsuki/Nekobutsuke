#pragma once
#include "../system/CSprite.h"
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
    void Init();
    void Update(uint64_t dt);

    // ---------------------------------------------------------
    // レンダリング (Rendering)
    // ---------------------------------------------------------
    void Draw();

private:
    std::unique_ptr<CSprite> m_sprite;

    // --- 状態管理 ---
    float m_scrollOffset = 0.0f;

    // --- アニメーション・描画パラメータ ---
    static constexpr float SCROLL_SPEED = 0.2f;  // スクロール速度
    static constexpr float TILE_REPEAT = 10.0f; // テクスチャの繰り返し回数
};