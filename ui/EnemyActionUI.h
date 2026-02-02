#pragma once
#include "../system/CSprite.h"
#include <vector>
#include <memory>

class GameContext;

class EnemyActionUI {
public:
    void Init(GameContext* context);
    void Update(float dt);

    // 描画関数：敵の頭上のワールド座標と行動順（1から開始）を渡す
    void Draw(const Vector3& worldPos, int order);

private:
    GameContext* m_context = nullptr;

    // 数字（1, 2, 3）のSpriteを格納するベクター
    std::vector<std::unique_ptr<CSprite>> m_numSprites;

    // --- アニメーションパラメータ ---
    enum class AnimState {
        Entrance,   // 登場（拡大しながら出現）
        Idle,       // 待機
        Shake       // 周期的な揺れ
    };

    AnimState m_state = AnimState::Entrance;
    float m_animTimer = 0.0f;           // 現在のアニメーション状態のタイマー
    float m_intervalTimer = 0.0f;       // 10秒ごとの周期タイマー

    const float ENTRANCE_DURATION = 0.5f; // 登場アニメーションの再生時間
    const float SHAKE_DURATION = 0.5f;    // 揺れアニメーションの再生時間
    const float SHAKE_INTERVAL = 10.0f;   // 揺れの間隔

    // 現在計算されたトランスフォーム属性
    float m_currentScale = 1.0f;
    float m_currentRotationZ = 0.0f;
};