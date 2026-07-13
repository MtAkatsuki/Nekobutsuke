#include "../../Core/main.h"
#include "TitleScene.h"
#include "../../System/CDirectInput.h"
#include "../../System/SceneManager.h"
#include "GameOverScene.h"
#include "../../System/SceneClassFactory.h"
#include "../../System/Audio/AudioManager.h"
#include "../../System/BackgroundTransition.h"

namespace {
    // --- 定数定義 ---
    constexpr float INPUT_LOCK_DURATION = 1.0f; // 遷移直後の誤操作防止(1秒)
}

void GameOverScene::Init() {
    if (!m_image) {
        m_image = std::make_unique<CSprite>(SCREEN_WIDTH, SCREEN_HEIGHT, "Assets/texture/gameover.png");
    }
    m_inputDelayTimer = 0.0f;

    AudioManager::GetInstance().PlayBGM("Over", false, 1.0f);
}

void GameOverScene::Update(float deltaSeconds) {

    m_inputDelayTimer += deltaSeconds;

    if (m_inputDelayTimer < INPUT_LOCK_DURATION) {
        return;
    }

    if (CDirectInput::GetInstance().IsAnyKeyTriggered()) {
        SceneManager::GetInstance().SetCurrentScene(
            "TitleScene",
            std::make_unique<BackgroundTransition>()
        );
    }
}

void GameOverScene::Dispose() {
}

void GameOverScene::Draw(float /*deltaSeconds*/) {
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