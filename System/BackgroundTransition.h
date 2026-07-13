#pragma once
#include "../System/SceneManager.h"
#include "SceneTransition.h"
#include "../GamePlay/Scene/Background.h"
#include <memory>

// =========================================================
// BackgroundTransition クラス
// シーン遷移時に、専用の背景画像（青ストライプ等）をフェードインさせ、
// スクロール速度を徐々に減速（イーズアウト）させながら画面を覆うトランジション演出。
// =========================================================
class BackgroundTransition : public SceneTransition {
private:
    // --- 遷移の進行状態 (State Machine) ---
    enum class Phase {
        Idle,       // 待機中 / 終了状態
        FadeInBg,   // 背景をフェードインして画面を覆う
        Scrolling,  // スクロール速度を減速させながら演出を見せる
        WaitSwap,   // 完全に覆い尽くし、裏側でのシーン切り替え(Swap)完了を待つ
        FadeOutBg   // 新シーンのロード後、背景をフェードアウトして完了
    };

public:
    // fadeDurationSeconds: フェードイン・アウトにかける時間（秒）
    // scrollDurationSeconds: スクロールが停止するまでの減速時間（秒）
    explicit BackgroundTransition(float fadeDurationSeconds = 1.0f, float scrollDurationSeconds = 4.0f);
    ~BackgroundTransition() override = default;

    // ---------------------------------------------------------
    // ライフサイクル・更新 (Lifecycle & Update)
    // ---------------------------------------------------------
    void Start() override;
    void Update(float deltaSeconds) override;
    void Draw() override;

    // ---------------------------------------------------------
    // フロー制御 (Flow)
    // ---------------------------------------------------------
    bool isFinished() const override { return m_phase == Phase::Idle; }
    bool canSwap() const override { return m_phase == Phase::WaitSwap; }
    void OnSceneSwapped() override;

private:
    std::unique_ptr<Background> m_bg;
    Phase m_phase = Phase::Idle;

    // --- アニメーション計算用パラメータ（すべて秒単位） ---
    float m_alpha = 0.0f;
    float m_fadeDuration;
    float m_scrollDuration;
    float m_elapsed = 0.0f;
};