#include "../../Core/main.h"
#include "TitleScene.h"
#include "../../System/CDirectInput.h"
#include "../../System/SceneManager.h"
#include "GameClearScene.h"
#include "../../System/SceneClassFactory.h"
#include "../../System/Audio/AudioManager.h"
#include "../../System/BackgroundTransition.h"
#include "../../Core/DebugLog.h"

namespace {
    // --- 定数定義  ---
    constexpr float INPUT_LOCK_DURATION = 1.0f; // 遷移直後の誤操作防止(1秒)
    constexpr float FADE_IN_OUT_DURATION_SEC = 1.0f;// フェードイン・アウトの合計時間（秒）
    constexpr float BACKGROUND_TRANSITION_TIME_SEC = 4.0f;// 背景遷移のスクロール時間（秒）
}

void GameClearScene::Init() {

    if (!m_image) {
        m_image = std::make_unique<CSprite>(SCREEN_WIDTH, SCREEN_HEIGHT, "Assets/texture/gameclear.png");
    }

    m_inputDelayTimer = 0.0f;

    AudioManager::GetInstance().PlayBGM("Clear", false, 1.0f);

    if (m_image) {
        DBG_ERROR("=== GameClearScene m_image Created Successfully ===");
    }
    else {
        DBG_ERROR("=== FATAL: m_image Creation FAILED ===");
    }
}

void GameClearScene::Update(float deltaSeconds) {

    m_inputDelayTimer += deltaSeconds;

    if (m_inputDelayTimer < INPUT_LOCK_DURATION) {
        return;
    }

    if (CDirectInput::GetInstance().IsAnyKeyTriggered()) {
        SceneManager::GetInstance().SetCurrentScene(
            "TitleScene",
            std::make_unique<BackgroundTransition>(FADE_IN_OUT_DURATION_SEC, BACKGROUND_TRANSITION_TIME_SEC)
        );
    }
}

void GameClearScene::Dispose() {
}

void GameClearScene::Draw(float /*deltaSeconds*/) {
    if (!m_image) {
        DBG_ERROR("[Warning] GameClearScene::draw called but m_image is NULL!");
        return;
    }

    Renderer::SetUISamplerMode(true);
    m_image->Draw(
        Vector3(1.0f, 1.0f, 1.0f),
        Vector3(0.0f, 0.0f, 0.0f),
        Vector3(SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f, 0.0f)
    );
    Renderer::SetUISamplerMode(false);
}

REGISTER_CLASS(GameClearScene);