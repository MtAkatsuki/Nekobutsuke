#pragma once

#include "../system/IScene.h"
#include "../system/CSprite.h"
#include "../system/camera.h"
#include "../system/FadeTransition.h"

/**
 * @brief タイトル画面を表すシーン
 *
 * ゲーム起動時に表示されるタイトルロゴ、メニュー、操作説明などを担当。
 * 入力を受けてメインゲームシーンへの遷移処理なども実装される。
 */
class GameClearScene : public IScene {
public:
	/// @brief コピーコンストラクタは使用不可
	GameClearScene(const GameClearScene&) = delete;

	/// @brief 代入演算子も使用不可
	GameClearScene& operator=(const GameClearScene&) = delete;

	/**
	 * @brief コンストラクタ
	 *
	 * カメラや画像スプライト、遷移演出の初期化を行う。
	 */
	explicit GameClearScene();

	/**
	 * @brief 毎フレームの更新処理
	 * @param deltatime 前フレームからの経過時間（マイクロ秒）
	 *
	 * 入力処理、アニメーション、遷移タイミングなどの制御を行う。
	 */
	void update(uint64_t deltatime) override;

	/**
	 * @brief 毎フレームの描画処理
	 * @param deltatime 前フレームからの経過時間（マイクロ秒）
	 *
	 * タイトルロゴや背景などのスプライト描画を行う。
	 */
	void draw(uint64_t deltatime) override;

	/**
	 * @brief シーンの初期化処理
	 *
	 * スプライトの生成、カメラ設定、音声再生など、表示に必要な準備を行う。
	 */
	void Init() override;

	/**
	 * @brief シーンの終了処理
	 *
	 * リソースの解放など、他のシーンへの遷移前に必要な処理を行う。
	 */
	void dispose() override;

private:


	/// @brief シーン遷移演出（例：フェードアウト）
	std::unique_ptr<SceneTransition> m_transition;

	/// @brief タイトルロゴや背景などのスプライト
	std::unique_ptr<CSprite> m_image;

	// 誤操作による即時スキップを防止するための入力遅延タイマー
	float m_inputDelayTimer = 0.0f;
	const float INPUT_LOCK_DURATION = 1.0f; // 1秒間ロック
};
