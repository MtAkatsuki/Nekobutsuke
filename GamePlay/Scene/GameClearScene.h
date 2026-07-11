#pragma once

#include "../../System/IScene.h"
#include "../../System/CSprite.h"
#include "../../System/Camera.h"
#include "../../System/FadeTransition.h"
#include <memory>

// =========================================================
// GameClearScene クラス
// ステージクリア時の画面表示と、タイトルへの遷移を管理。
// =========================================================
class GameClearScene : public IScene {
public:
    GameClearScene() = default;
    GameClearScene(const GameClearScene&) = delete;
    GameClearScene& operator=(const GameClearScene&) = delete;

    // ---------------------------------------------------------
    // ライフサイクル (Lifecycle)
    // ---------------------------------------------------------
    void Init() override;
    void Dispose() override;
    void Update(float deltaSeconds) override;

    // ---------------------------------------------------------
    // レンダリング (Rendering)
    // ---------------------------------------------------------
    void Draw(float deltaSeconds) override;

private:
    std::unique_ptr<SceneTransition> m_transition;
    std::unique_ptr<CSprite> m_image;

    // --- 状態管理 ---
    // 誤操作による即時スキップを防止するためのタイマー
    float m_inputDelayTimer = 0.0f;
};