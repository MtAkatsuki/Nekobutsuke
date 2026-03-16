#include "../main.h"
#include "titlescene.h"
#include "../system/CDirectInput.h"
#include "../system/scenemanager.h"
#include "../system/SceneClassFactory.h"
#include "../manager/AudioManager.h"
#include "../system/BackgroundTransition.h"
#include <cmath>

namespace {
    // --- 演出・定数定義 ---
	constexpr float FADE_IN_OUT_DURATION = 1000.0f;// フェードイン・アウトの合計時間（ms）
	constexpr float BackgroundTransitionTime = 4000.0f;// 背景遷移のスクロール時間（ms）
    constexpr float BLINK_SPEED       = 1.0f;
    constexpr float BLINK_MIN_ALPHA   = 0.3f; // 完全に消えないための下限値
    constexpr float BLINK_RANGE       = 0.7f; // 変動幅 (0.3 + 0.7 = 1.0)
    
    constexpr float HINT_POS_Y_RATIO  = 0.8f; // 画面下部への配置比率
}

void TitleScene::Init() {
    // nullptr の場合のみ生成（安全なリソース初期化）
    if (!m_image) {
        m_image = std::make_unique<CSprite>(SCREEN_WIDTH, SCREEN_HEIGHT, "assets/texture/title.png");
    }
    if (!m_hintSprite) {
        m_hintSprite = std::make_unique<CSprite>(677.0f, 369.0f, "assets/texture/ui/ui_press_enter.png");
    }

    m_blinkTimer = 0.0f;
    m_currentHintAlpha = 1.0f;

    // タイトルBGMの再生 (ループあり、1秒フェードイン)
    AudioManager::GetInstance().PlayBGM("Title", true, 1.0f);
}

void TitleScene::update(uint64_t deltatime) {
    float deltaSeconds = static_cast<float>(deltatime) / 1000.0f;

    // --- アニメーションの更新 ---
    m_blinkTimer += deltaSeconds;
    // サイン波による呼吸（明滅）エフェクトの計算
    float sinValue = std::abs(std::sin(m_blinkTimer * BLINK_SPEED));
    m_currentHintAlpha = BLINK_MIN_ALPHA + (BLINK_RANGE * sinValue);

    // --- 入力検知 (Any Key) ---
    bool hasAnyKeyPressed = false;
    // 主要なキーコード(1~255)を走査
    for (int i = 1; i < 256; i++) {
        if (CDirectInput::GetInstance().CheckKeyBufferTrigger(i)) {
            hasAnyKeyPressed = true;
            break;
        }
    }

    if (hasAnyKeyPressed) {
        SceneManager::GetInstance().SetCurrentScene(
            "GameScene",
            std::make_unique<BackgroundTransition>(FADE_IN_OUT_DURATION, BackgroundTransitionTime)
        );
    }
}

void TitleScene::dispose() {

}

void TitleScene::draw(uint64_t deltatime) {
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