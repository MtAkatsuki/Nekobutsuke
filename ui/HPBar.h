#pragma once
#include "../system/CSprite.h"
#include <memory>
#include <string>

class GameContext;

class HPBar
{
public:
    enum class BarType {
        Green, // プレーヤー
        Red    // エネミー
    };

    HPBar() {};
    ~HPBar() {};

    void Init(GameContext* context);

    // 描画
	// worldPos: ユニット足元のワールド座標
    void Draw(const Vector3& worldPos, int currentHP, int maxHP, BarType type);

private:
    GameContext* m_context = nullptr;

    // アセット
	std::unique_ptr<CSprite> m_bgSprite;    // ベース板
	std::unique_ptr<CSprite> m_greenSprite; // 緑ブロック
	std::unique_ptr<CSprite> m_redSprite;   // 赤ブロック

 // ベース板表示サイズ
    const float DISPLAY_BG_WIDTH = 102.0f;
    const float DISPLAY_BG_HEIGHT = 15.0f;

    // パディング（余白）：HPバーがベース板の枠線を隠さないようにするため
    const float PADDING_X = 4.0f;
    const float PADDING_Y = 4.0f;
    const float GAP = 1.0f; // ブロック間の隙間

	// ブロック元のサイズ、スケール計算用
	//maxHpに応じてスケール調整するため
    float m_texBlockWidth = 0.0f;
    float m_texBlockHeight = 0.0f;
};