#include "BackgroundTransition.h"
#include <algorithm> // for std::max

namespace {
    // --- 演出用定数定義 ---
    constexpr float MAX_SCROLL_SPEED = 0.2f; // スクロール開始時の初期速度
    const char* TRANSITION_BG_PATH = "Assets/texture/bg_stripe_blue.png";
}

BackgroundTransition::BackgroundTransition(float fadeDurationSeconds, float scrollDurationSeconds)
    : m_fadeDuration(fadeDurationSeconds), m_scrollDuration(scrollDurationSeconds) {
}

void BackgroundTransition::Start() {
    m_bg = std::make_unique<Background>();
    m_bg->Init(TRANSITION_BG_PATH);

    m_alpha = 0.0f;
    m_elapsed = 0.0f;
    m_phase = Phase::FadeInBg;
}

void BackgroundTransition::Update(float deltaSeconds) {
    if (!m_bg) return;

    m_bg->Update(deltaSeconds);

    m_elapsed += deltaSeconds;

    switch (m_phase) {
    case Phase::FadeInBg: {
        m_alpha = m_elapsed / m_fadeDuration;
        if (m_elapsed >= m_fadeDuration) {
            m_alpha = 1.0f;
            m_elapsed = 0.0f;
            m_phase = Phase::Scrolling;
        }
        break;
    }

    case Phase::Scrolling: {
        m_alpha = 1.0f;

        // 進行度 (0.0f ~ 1.0f)
        float progress = m_elapsed / m_scrollDuration;

        // 徐々に減速するイーズアウト（Ease-Out）計算
        float currentSpeed = MAX_SCROLL_SPEED * (1.0f - progress);

        // 速度がマイナスにならないよう安全にクランプ
        m_bg->SetScrollSpeed(std::max(0.0f, currentSpeed));

        if (m_elapsed >= m_scrollDuration) {
            m_bg->SetScrollSpeed(0.0f);
            m_phase = Phase::WaitSwap;
        }
        break;
    }

    case Phase::WaitSwap: {
        m_alpha = 1.0f;
        // 備考: SceneManager から OnSceneSwapped() が呼ばれるまでこの状態を維持する
        break;
    }

    case Phase::FadeOutBg: {
        m_alpha = 1.0f - (m_elapsed / m_fadeDuration);
        if (m_elapsed >= m_fadeDuration) {
            m_alpha = 0.0f;
            m_phase = Phase::Idle; // 遷移完了
        }
        break;
    }

    case Phase::Idle:
    default:
        break;
    }
}

void BackgroundTransition::OnSceneSwapped() {
    // SceneManager側で新しいシーンの Init が完了したタイミングで呼ばれる
    m_phase = Phase::FadeOutBg;
    m_elapsed = 0.0f;
}

void BackgroundTransition::Draw() {
    // 待機中（Idle）以外、かつ背景が存在する場合のみ描画コマンドを発行
    if (m_phase != Phase::Idle && m_bg) {
        m_bg->SetAlpha(m_alpha);
        m_bg->Draw();
    }
}