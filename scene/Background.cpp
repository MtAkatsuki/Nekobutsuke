#include "Background.h"
#include "../Application.h"
#include "../utility/WorldToScreen.h"
#include "../system/commontypes.h" // for pi
#include <cmath>

namespace {
	// --- 背景描画用・定数 ---
	constexpr float BG_SIZE = 2500.0f; // 斜め回転時も画面を覆い尽くせる余裕を持たせたサイズ
	constexpr float UV_OVERFLOW_LIMIT = 100.0f;  // UV座標のオーバーフロー防止閾値
	constexpr float BG_Z_POSITION = 0.5f;    // 背景の深度（最奥）

	constexpr float ROTATION_RAD = -PI / 4.0f;   // -45度の斜め配置
}

void Background::Init() {
	m_sprite = std::make_unique<CSprite>(BG_SIZE, BG_SIZE, "assets/texture/bg_stripe.png");
}

void Background::Update(uint64_t dt) {
    if (!m_sprite) return;

    float deltaSeconds = static_cast<float>(dt) / 1000.0f;

    // スクロールオフセットの加算とオーバーフロー防御
    m_scrollOffset += SCROLL_SPEED * deltaSeconds;
    if (m_scrollOffset > UV_OVERFLOW_LIMIT) {
        m_scrollOffset -= UV_OVERFLOW_LIMIT;
    }

    // --- UV座標の計算と適用 ---
    float u_start = 0.0f;
    float v_start = m_scrollOffset;
    float u_end = TILE_REPEAT;
    float v_end = m_scrollOffset + TILE_REPEAT;

    Vector2 uv[4] = {
        Vector2(u_start, v_start), // 左上
        Vector2(u_end,   v_start), // 右上
        Vector2(u_start, v_end),   // 左下
        Vector2(u_end,   v_end)    // 右下
    };

    m_sprite->ModifyUV(uv);
}

void Background::Draw() {
    if (!m_sprite) return;

    float screenW = static_cast<float>(Application::GetWidth());
    float screenH = static_cast<float>(Application::GetHeight());

    Vector3 centerPos(screenW / 2.0f, screenH / 2.0f, BG_Z_POSITION);
    Vector3 scale(1.0f, 1.0f, 1.0f);
    Vector3 rot(0.0f, 0.0f, ROTATION_RAD);

    m_sprite->Draw(scale, rot, centerPos);
}