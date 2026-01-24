#include "Background.h"
#include "../Application.h"
#include "../utility/WorldToScreen.h"
#include <cmath>

#ifndef PI
#define PI 3.14159265f
#endif

void Background::Init()
{
	// 巨大な Sprite を作成
	// 斜めを覆うために十分な大きさが必要
	float bigSize = 2500.0f;

	m_sprite = std::make_unique<CSprite>(bigSize, bigSize, "assets/texture/bg_stripe.png");

	// 初期化は中心位置で
	float screenW = (float)Application::GetWidth();
	float screenH = (float)Application::GetHeight();
}

void Background::Update(uint64_t dt)
{
	float deltaSeconds = static_cast<float>(dt) / 1000.0f;
	// --- 更新スクロールオフセット ---
	m_scrollOffset += SCROLL_SPEED * deltaSeconds;

	// オーバーーフロー防止
	if (m_scrollOffset > 100.0f) m_scrollOffset -= 100.0f;

	// --- UVを設置 ---
	// 左上 (u, v) -> 右下 (u + repeat, v + repeat)
	// offsetを加算してスクロールさせる
	float u_start = 0.0f;
	float v_start = m_scrollOffset;
	float u_end = TILE_REPEAT;
	float v_end = m_scrollOffset + TILE_REPEAT;
	
	Vector2 uv[4];
	uv[0] = Vector2(u_start, v_start); // 左上
	uv[1] = Vector2(u_end, v_start);   // 右上
	uv[2] = Vector2(u_start, v_end);   // 左下
	uv[3] = Vector2(u_end, v_end);     // 右下

	m_sprite->ModifyUV(uv);
}

void Background::Draw()
{
	if (!m_sprite) return;

	float screenW = (float)Application::GetWidth();
	float screenH = (float)Application::GetHeight();

	// スクリーン中央に配置
	Vector3 centerPos = Vector3(screenW / 2.0f, screenH / 2.0f, 0.5f); // Z=0.5f 最後に確保する

	// スケール：そのまま
	Vector3 scale = Vector3(1.0f, 1.0f, 1.0f);

	// 回転：-４５度、斜めに配置
	Vector3 rot = Vector3(0.0f, 0.0f, -PI / 4.0f ); 

	m_sprite->Draw(scale, rot, centerPos);
}