#include "TutorialUI.h"
#include "../system/CDirectInput.h"
#include "../Application.h"
#include <iostream>

namespace {
    const float ANIM_SPEED = 5.0f;  // フェードイン・アウトの速度
    const float TEX_WIDTH = 1009.0f;
    const float TEX_HEIGHT = 573.0f;
}

void TutorialUI::Init() {
    m_images.push_back(std::make_unique<CSprite>(1009, 573, "assets/texture/ui/tutorial_01.png"));
    m_images.push_back(std::make_unique<CSprite>(1009, 573, "assets/texture/ui/tutorial_02.png"));
    m_images.push_back(std::make_unique<CSprite>(1009, 573, "assets/texture/ui/tutorial_03.png"));

    m_currentIndex = 0;
    m_state = State::APPEARING;
    m_scale = 0.0f;
    m_isAllFinished = false;

    m_centerX = static_cast<float>(Application::GetWidth()) / 2.0f;
    m_centerY = static_cast<float>(Application::GetHeight()) / 2.0f;
}

void TutorialUI::Update(float deltaSeconds) {
    if (m_isAllFinished || m_currentIndex >= m_images.size()) return;

    switch (m_state) {
    case State::APPEARING:
        // 拡大アニメーション
        m_scale += ANIM_SPEED * deltaSeconds;
        if (m_scale >= 1.0f) {
            m_scale = 1.0f;
            m_state = State::WAITING; // アニメーション終了、待機状態へ
        }
        break;

    case State::WAITING:
        // Enterキーの入力を待機
        if (CDirectInput::GetInstance().CheckKeyBufferTrigger(DIK_RETURN)) {
            m_state = State::DISAPPEARING; // 消失アニメーションを開始
        }
        break;

    case State::DISAPPEARING:
        // 縮小アニメーション
        m_scale -= ANIM_SPEED * deltaSeconds;
        if (m_scale <= 0.0f) {
            m_scale = 0.0f;
            m_state = State::FINISHED; // 現在の画像の表示が終了
        }
        break;

    case State::FINISHED:
        // 次の画像へ切り替え
        m_currentIndex++;
        if (m_currentIndex >= m_images.size()) {
            m_isAllFinished = true; // 全ての画像の再生が終了
        }
        else {
            // 次の画像を再生する準備
            m_state = State::APPEARING;
            m_scale = 0.0f;
        }
        break;
    }
}

void TutorialUI::Draw() {
    if (m_isAllFinished || m_currentIndex >= m_images.size()) return;

    if (m_images[m_currentIndex]) {
        Renderer::SetUISamplerMode(true);
        Renderer::SetBlendState(BS_ALPHABLEND);
        Renderer::SetDepthEnable(false);

        m_images[m_currentIndex]->Draw(
            Vector3(m_scale, m_scale, 1.0f),
            Vector3(0.0f, 0.0f, 0.0f),
            Vector3(m_centerX, m_centerY, 0.0f) 
        );

        Renderer::SetDepthEnable(true);
        Renderer::SetBlendState(BS_NONE);
        Renderer::SetUISamplerMode(false);
    }
}