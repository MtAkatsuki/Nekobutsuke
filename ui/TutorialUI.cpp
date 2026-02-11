#include "TutorialUI.h"
#include "../system/CDirectInput.h"
#include "../Application.h"

TutorialUI::TutorialUI() {
}

TutorialUI::~TutorialUI() {
    m_images.clear();
}

void TutorialUI::Init() {
    // 3枚のチュートリアル画像を読み込み
    // 各パスに対応する png ファイルが存在することを確認してください
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
    if (m_isAllFinished) return;

    // 安全チェック：インデックスの範囲外参照を防止
    if (m_currentIndex >= m_images.size()) {
        m_isAllFinished = true;
        return;
    }

    switch (m_state) {
    case State::APPEARING:
        // 拡大アニメーション
        m_scale += m_animSpeed * deltaSeconds;
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
        m_scale -= m_animSpeed * deltaSeconds;
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
    if (m_isAllFinished) return;
    if (m_currentIndex >= m_images.size()) return;

    // 必要に応じて、背景を暗くするための半透明の黒いマスクをここで描画
    // ...

    // 現在の画像を描画
    // CSprite::Draw が (スケール, 回転, 位置) を受け取る引数構成であると仮定
    if (m_images[m_currentIndex]) {
        // アルファブレンドを有効にして透明度付き画像をサポート
        Renderer::SetBlendState(BS_ALPHABLEND);
        Renderer::SetDepthEnable(false); // UIは通常デプステスト（深度テスト）を不要とする

        m_images[m_currentIndex]->Draw(
            Vector3(m_scale, m_scale, 1.0f),     // スケール
            Vector3(0.0f, 0.0f, 0.0f),           // 回転
            Vector3(m_centerX, m_centerY, 0.0f)  // 位置 (画面中心)
        );

        Renderer::SetDepthEnable(true);
        Renderer::SetBlendState(BS_NONE);
    }
}