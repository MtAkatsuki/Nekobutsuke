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
    void Update(float dt);
    // 描画
    // previewDamage: ダメージのプレビュー量（右から左へ点滅させるダメージ予定分）
    void Draw(const Vector3& worldPos, int currentHP, int maxHP, int previewDamage = 0);
public:
    // HPバーの位置調整用オフセット（imgui）
    static float s_hpBarOffsetY;
    static float s_hpBarTexSize;
    static float s_hpBarGap;
private:
    GameContext* m_context = nullptr;

    // アセット (色は区別せず、ハート型の枠と塗りつぶしの組み合わせに変更)
    std::unique_ptr<CSprite> m_frameSprite; // 底枠（空のハート）
    std::unique_ptr<CSprite> m_fullSprite;  // 赤いハート（中身）

    const float GAP = 2.0f;     // ハート同士の間隔
    float m_texWidth = 24.0f;   // ハートのテクスチャサイズ（幅）
    float m_texHeight = 24.0f;  // ハートのテクスチャサイズ（高さ）

	// 点滅制御用タイマー
    float m_blinkTimer = 0.0f;
    bool m_blinkState = true;
};