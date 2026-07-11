#pragma once

#include "../../System/IScene.h"
#include "../../System/Camera.h"
#include "../../System/scenemanager.h"
#include "../../System/fadetransition.h"
#include "../../System/CSprite.h"
#include <memory>

// =========================================================
// TitleScene クラス
// ゲーム起動時のタイトルロゴ表示と、メインゲームへの遷移を管理する。
// =========================================================
class TitleScene : public IScene {
public:
    TitleScene() = default;
    TitleScene(const TitleScene&) = delete;
    TitleScene& operator=(const TitleScene&) = delete;

    // ---------------------------------------------------------
    // ライフサイクル (Lifecycle)
    // ---------------------------------------------------------
    void Init() override;
    void dispose() override;
    void update(uint64_t deltatime) override;

    // ---------------------------------------------------------
    // レンダリング (Rendering)
    // ---------------------------------------------------------
    void draw(uint64_t deltatime) override;

private:
    std::unique_ptr<SceneTransition> m_transition;

    // --- アセット ---
    std::unique_ptr<CSprite> m_image;
    std::unique_ptr<CSprite> m_hintSprite;

    // --- アニメーション・状態管理 ---
    float m_blinkTimer = 0.0f;
    float m_currentHintAlpha = 1.0f; // Updateで計算し、Drawで適用するアルファ値キャッシュ
};