#pragma once

#include "../../System/IScene.h"
#include "../../System/CSprite.h"
#include "../../System/Camera.h"
#include "../../System/FadeTransition.h"
#include <memory>

// =========================================================
// GameOverScene クラス
// プレイヤー敗北時のゲームオーバー画面と、タイトルへの遷移を管理。
// =========================================================
class GameOverScene : public IScene {
public:
    GameOverScene() = default;
    GameOverScene(const GameOverScene&) = delete;
    GameOverScene& operator=(const GameOverScene&) = delete;

    // ---------------------------------------------------------
    // ライフサイクル (Lifecycle)
    // ---------------------------------------------------------
    void Init() override;
    void Dispose() override;
    void Update(uint64_t deltatime) override;

    // ---------------------------------------------------------
    // レンダリング (Rendering)
    // ---------------------------------------------------------
    void Draw(uint64_t deltatime) override;

private:
    std::unique_ptr<SceneTransition> m_transition;
    std::unique_ptr<CSprite> m_image;

    // --- 状態管理 ---
    // 誤操作による即時スキップを防止するためのタイマー
    float m_inputDelayTimer = 0.0f;
};