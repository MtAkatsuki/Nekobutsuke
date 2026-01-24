#pragma once
#include <string>

/**
 * @brief シーン遷移演出のインターフェース
 *
 * フェードイン・フェードアウトやクロスフェードなど、視覚的なシーン切り替え演出を抽象化したクラス。
 * SceneManager によって利用され、シーンの切り替え時に演出処理を挟むための基底クラス。
 */
class SceneTransition {
public:
    /// @brief デフォルトの仮想デストラクタ（継承クラスの安全な破棄のため）
    virtual ~SceneTransition() = default;

    /**
     * @brief 遷移演出の開始処理
     *
     * 遷移が開始されたときに一度だけ呼ばれる。初期化や設定に利用。
     */
    virtual void start() = 0;

    /**
     * @brief 遷移演出の更新処理
     * @param deltaTime 前フレームからの経過時間（マイクロ秒など）
     *
     * アニメーションや状態の更新に使用する。
     */
    virtual void update(uint64_t deltaTime) = 0;

    /**
     * @brief 遷移演出の描画処理
     *
     * 通常のシーン描画後に重ねて表示される。
     */
    virtual void draw() = 0;

    /**
     * @brief 演出が完了したかどうかを返す
     * @return true 完了した（シーン切り替え可）
     * @return false 演出中（まだ切り替え不可）
     *
     * SceneManager はこれを監視して、シーンを切り替えるタイミングを制御する。
     */
    virtual bool isFinished() const = 0;
    //シーン切り替えの準備はできたのか
    virtual bool canSwap() const = 0;
    //シーン切り替え完了通知：以降のアニメーション（例：FadeIn）を実行
    virtual void onSceneSwapped() = 0;
};
