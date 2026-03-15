#include "../main.h"
#include "titlescene.h"
#include "../system/CDirectInput.h"
#include "../system/scenemanager.h"
#include "GameOverScene.h"
#include "../system/SceneClassFactory.h"
#include "../manager/AudioManager.h"

namespace {
    // --- íËêîíËã` ---
    constexpr float FADE_OUT_DURATION = 1000.0f;
    constexpr float INPUT_LOCK_DURATION = 1.0f; // ëJà⁄íºå„ÇÃåÎëÄçÏñhé~(1ïb)
}

void GameOverScene::Init() {
    if (!m_image) {
        m_image = std::make_unique<CSprite>(SCREEN_WIDTH, SCREEN_HEIGHT, "assets/texture/gameover.png");
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

    if (CDirectInput::GetInstance().CheckKeyBufferTrigger(DIK_RETURN)) {
        SceneManager::GetInstance().SetCurrentScene(
            "TitleScene",
            std::make_unique<FadeTransition>(FADE_OUT_DURATION, FadeTransition::Mode::FadeInOut)
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