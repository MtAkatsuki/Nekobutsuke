#pragma once
#include "SceneManager.h"
#include "SceneTransition.h"

//FdaeTransitionは画面の明暗だけ管理する

/**
 * @brief フェード演出によるシーン遷移を行うクラス
 *
 * モードに応じて、フェードインのみ／フェードアウトのみ／その両方の演出が可能。
 * シーン切り替え時に黒い矩形を使って画面の明暗を調整する。
 */
class FadeTransition : public SceneTransition {
public:
    /**
    * @brief フェード演出のモード
     */
    enum class Mode { FadeInOnly, FadeOutOnly,FadeInOut };

private:
    float m_alpha = 0.0f;
    float m_duration;      // フェード時間（秒）
    float m_elapsed = 0.0f; // 経過時間（秒）
    Mode m_mode;

    /**
    * @brief 現在のフェードフェーズ
    */
    enum class Phase {
        Idle,
        FadeOut,
        Wait,//SceneManagerのシーン遷移を待つ
        FadeIn
    } m_phase = Phase::Idle;

public:
    /**
    * @brief コンストラクタ
     *
    * @param durationSeconds フェード時間（秒）
    * @param mode フェードモード（デフォルトは FadeInOut）
    */
    explicit FadeTransition(float durationSeconds, Mode mode = Mode::FadeInOut)
        : m_duration(durationSeconds), m_mode(mode) {}

    /**
     * @brief フェード演出の開始処理
    *
    * @param nextSceneName 遷移先のシーン名
    */
    void Start() override;

    /**
     * @brief フェード演出の更新処理
    *
    * @param deltaSeconds 前フレームからの経過時間（秒）
    */
    void Update(float deltaSeconds) override;

    /**
    * @brief 黒フェード矩形の描画処理
    */
    void Draw() override;

    /**
     * @brief フェード演出の完了判定
    *
    * @return true フェード演出が終了している
    * @return false 演出中
    */
    bool isFinished() const override;

    bool canSwap() const override { return m_phase == Phase::Wait; }
    void OnSceneSwapped() override;
};
