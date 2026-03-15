#pragma once
#include "../system/CSprite.h"
#include <vector>
#include <memory>

// =========================================================
// TutorialUI クラス
// ゲーム開始前にチュートリアル画像を順番に表示する
// =========================================================
class TutorialUI {
public:
    TutorialUI() = default;
    ~TutorialUI() = default;

    void Init();
    void Update(float deltaSeconds);
    void Draw();

    bool IsAllFinished() const { return m_isAllFinished; }

private:
    enum class State {
        APPEARING,    // 拡大して出現
        WAITING,      // 入力待ち
        DISAPPEARING, // 縮小して消失
        FINISHED      // 現在の画像終了
    };

    std::vector<std::unique_ptr<CSprite>> m_images;
    int m_currentIndex = 0;

    State m_state = State::APPEARING;
    bool m_isAllFinished = false;

    float m_scale = 0.0f;
    float m_centerX = 0.0f;
    float m_centerY = 0.0f;
};