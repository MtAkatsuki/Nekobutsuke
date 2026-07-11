#include "../../Core/main.h"
#include "titlescene.h"
#include "../../System/CDirectInput.h"
#include "../../System/scenemanager.h"
#include "GameOverScene.h"
#include "../../System/SceneClassFactory.h"
#include "../../System/Audio/AudioManager.h"
#include "../../System/BackgroundTransition.h"

namespace {
    // --- 定数定義 ---
    constexpr float FADE_OUT_DURATION = 1000.0f;
    constexpr float INPUT_LOCK_DURATION = 1.0f; // 遷移直後の誤操作防止(1秒)
    constexpr float FADE_IN_OUT_DURATION = 1000.0f;// フェードイン・アウトの合計時間（ms）
    constexpr float BackgroundTransitionTime = 4000.0f;// 背景遷移のスクロール時間（ms）
}

void GameOverScene::Init() {
    if (!m_image) {
        m_image = std::make_unique<CSprite>(SCREEN_WIDTH, SCREEN_HEIGHT, "Assets/texture/gameover.png");
    }
    m_inputDelayTimer = 0.0f;

    AudioManager::GetInstance().PlayBGM("Over", false, 1.0f);
}

void GameOverScene::update(uint64_t deltatime) {
    float deltaSeconds = static_cast<float>(deltatime) / 1000.0f;

    m_inputDelayTimer += deltaSeconds;

    if (m_inputDelayTimer < INPUT_LOCK_DURATION) {
        return;
    }

    bool hasAnyKeyPressed = false;
    for (int i = 1; i < 256; i++) {
        if (CDirectInput::GetInstance().CheckKeyBufferTrigger(i)) {
            hasAnyKeyPressed = true;
            break;
        }
    }

    if (hasAnyKeyPressed) {
        SceneManager::GetInstance().SetCurrentScene(
            "TitleScene",
            std::make_unique<BackgroundTransition>(FADE_IN_OUT_DURATION, BackgroundTransitionTime)
        );
    }
}

void GameOverScene::dispose() {
}

void GameOverScene::draw(uint64_t deltatime) {
    if (!m_image) return;

    Renderer::SetUISamplerMode(true);
    m_image->Draw(
        Vector3(1.0f, 1.0f, 1.0f),
        Vector3(0.0f, 0.0f, 0.0f),
        Vector3(SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f, 0.0f)
    );
    Renderer::SetUISamplerMode(false);
}

REGISTER_CLASS(GameOverScene);