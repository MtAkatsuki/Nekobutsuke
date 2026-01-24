#pragma once
#include "../system/CSprite.h"
#include <memory>

class Background
{
public:
	Background() {}
	~Background() {}

	void Init();
	void Update(uint64_t dt);
	void Draw();

private:
	std::unique_ptr<CSprite> m_sprite;

	// UVスクロール用オフセット
	float m_scrollOffset = 0.0f;
	const float SCROLL_SPEED = 0.2f; // スクロール速度

	// テクスチャの繰り返し回数（スクリーン全体に見えるように調整）
	const float TILE_REPEAT = 10.0f;
};