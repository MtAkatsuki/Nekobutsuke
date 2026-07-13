#include "../../Core/main.h"
#include "TitleScene.h"
#include "../../System/CDirectInput.h"
#include "../../System/SceneManager.h"
#include "../../System/SceneClassFactory.h"
#include "../../System/Audio/AudioManager.h"
#include "../../System/BackgroundTransition.h"
#include <cmath>

namespace {
    // --- 演出・定数定義 ---
	constexpr float FADE_IN_OUT_DURATION_SEC = 1.0f;// フェードイン・アウトの合計時間（秒）
	constexpr float BACKGROUND_TRANSITION_TIME_SEC = 1.0f;// 背景遷移のスクロール時間（秒）
    constexpr float BLINK_SPEED       = 1.0f;
    constexpr float BLINK_MIN_ALPHA   = 0.3f; // 完全に消えないための下限値
    constexpr float BLINK_RANGE       = 0.7f; // 変動幅 (0.3 + 0.7 = 1.0)
    
    constexpr float HINT_POS_Y_RATIO  = 0.8f; // 画面下部への配置比率

    constexpr float BGM_FADE_TIME = 1.0f;     // タイトルBGMのフェードイン時間（秒）

    // ヒントスプライトのテクスチャ実寸（px）
    constexpr float HINT_TEX_W = 677.0f;
    constexpr float HINT_TEX_H = 369.0f;
}

void TitleScene::Init() {
    // nullptr の場合のみ生成（安全なリソース初期化）
    if (!m_image) {
        m_image = std::make_unique<CSprite>(SCREEN_WIDTH, SCREEN_HEIGHT, "Assets/texture/title.png");
    }
    if (!m_hintSprite) {
        m_hintSprite = std::make_unique<CSprite>(HINT_TEX_W, HINT_TEX_H, "Assets/texture/ui/ui_press_enter.png");
    }

    m_blinkTimer = 0.0f;
    m_currentHintAlpha = 1.0f;

    // タイトルBGMの再生 (ループあり、フェードイン)
    AudioManager::GetInstance().PlayBGM("Title", true, BGM_FADE_TIME);
}

void TitleScene::Update(float deltaSeconds) {

    // --- アニメーションの更新 ---
    m_blinkTimer += deltaSeconds;
    // サイン波による呼吸（明滅）エフェクトの計算
    float sinValue = std::abs(std::sin(m_blinkTimer * BLINK_SPEED));
    m_currentHintAlpha = BLINK_MIN_ALPHA + (BLINK_RANGE * sinValue);

    if (CDirectInput::GetInstance().IsAnyKeyTriggered()) {
        SceneManager::GetInstance().SetCurrentScene(
            "GameScene",
            std::make_unique<BackgroundTransition>(FADE_IN_OUT_DURATION_SEC, BACKGROUND_TRANSITION_TIME_SEC)
        );
    }
}

void TitleScene::Dispose() {

}

void TitleScene::Draw(float /*deltaSeconds*/) {
    if (!m_image) return;

    Renderer::SetUISamplerMode(true);

    // 背景ロゴ描画
    m_image->Draw(
        Vector3(1.0f, 1.0f, 1.0f),
        Vector3(0.0f, 0.0f, 0.0f),
        Vector3(SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f, 0.0f)
    );

    // ヒント(Press Any Key)描画
    if (m_hintSprite) {
        MATERIAL mtrl;
        mtrl.Diffuse = Color(1.0f, 1.0f, 1.0f, m_currentHintAlpha); // Updateで計算済みの値を適用
        mtrl.TextureEnable = TRUE;
        m_hintSprite->ModifyMtrl(mtrl);

        float posX = SCREEN_WIDTH / 2.0f;
        float posY = SCREEN_HEIGHT * HINT_POS_Y_RATIO;

        // UI用ブレンドステート適用
        Renderer::SetBlendState(BS_ALPHABLEND);
        Renderer::SetDepthEnable(false);

        m_hintSprite->Draw(Vector3(1.0f, 1.0f, 1.0f), Vector3(0.0f, 0.0f, 0.0f), Vector3(posX, posY, 0.0f));

        Renderer::SetDepthEnable(true);
        Renderer::SetBlendState(BS_NONE);
    }

    Renderer::SetUISamplerMode(false);
}


REGISTER_CLASS(TitleScene);