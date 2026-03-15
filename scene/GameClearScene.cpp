#include "../main.h"
#include "titlescene.h"
#include "../system/CDirectInput.h"
#include "../system/scenemanager.h"
#include "GameClearScene.h"
#include "../system/SceneClassFactory.h"
#include "../manager/AudioManager.h"
#include <iostream>

namespace {
    // --- íËêîíËã`  ---
    constexpr float FADE_OUT_DURATION = 300.0f;
    constexpr float INPUT_LOCK_DURATION = 1.0f; // ëJà⁄íºå„ÇÃåÎëÄçÏñhé~(1ïb)
}

void GameClearScene::Init() {
    std::cerr << "=== GameClearScene::Init() CALLED ===" << std::endl;

    if (!m_image) {
        m_image = std::make_unique<CSprite>(SCREEN_WIDTH, SCREEN_HEIGHT, "assets/texture/gameclear.png");
    }

    m_inputDelayTimer = 0.0f;

    AudioManager::GetInstance().PlayBGM("Clear", false, 1.0f);

    if (m_image) {
        std::cerr << "=== GameClearScene m_image Created Successfully ===" << std::endl;
    }
    else {
        std::cerr << "=== FATAL: m_image Creation FAILED ===" << std::endl;
    }
}

void GameClearScene::update(uint64_t deltatime) {
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

void GameClearScene::dispose() {
}

void GameClearScene::draw(uint64_t deltatime) {
    if (!m_image) {
        std::cerr << "[Warning] GameClearScene::draw called but m_image is NULL!" << std::endl;
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