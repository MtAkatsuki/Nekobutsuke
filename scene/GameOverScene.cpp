#include "../main.h"
#include "titlescene.h"
#include "../system/CDirectInput.h"
#include "../system/scenemanager.h"
#include "GameOverScene.h"
#include "../system/SceneClassFactory.h"

/**
 * @brief タイトルシーンのコンストラクタ
 *
 *
 */
GameOverScene::GameOverScene()
{
}

/**
 * @brief クリアシーンの更新処理
 *
 * Enterキー（リターンキー）が押されたら、`TitleScene` へフェードアウト遷移する。
 *
 * @param deltatime 前フレームからの経過時間（マイクロ秒）
 */
void GameOverScene::update(uint64_t deltatime)
{
    float deltaSeconds = static_cast<float>(deltatime) / 1000.0f;

    // 経過時間をタイマーに加算
    m_inputDelayTimer += deltaSeconds;

    // ロック時間に達していない場合、入力を受け付けずにリターン
    if (m_inputDelayTimer < INPUT_LOCK_DURATION) {
        return;
    }

    if (CDirectInput::GetInstance().CheckKeyBufferTrigger(DIK_RETURN))
    {
  
        SceneManager::GetInstance().SetCurrentScene(
            "TitleScene",
            std::make_unique<FadeTransition>(1000.0f, FadeTransition::Mode::FadeInOut)
        );
    }
}

/**
 * @brief クリアシーンの描画処理
 *
 * クリア画像を画面中央に描画する。
 *
 * @param deltatime 前フレームからの経過時間（マイクロ秒）
 */
void GameOverScene::draw(uint64_t deltatime)
{
    m_image->Draw(
        Vector3(1.0f, 1.0f, 1.0f),
        Vector3(0.0f, 0.0f, 0.0f),
        Vector3(SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f, 0.0f)
    );
}

/**
 * @brief タイトルシーンの初期化処理
 *
 * シーンがアクティブになる際に一度だけ呼ばれる初期化処理。
 */
void GameOverScene::Init()
{
    if (!m_image) {
        m_image = std::make_unique<CSprite>(
            SCREEN_WIDTH,
            SCREEN_HEIGHT,
            "assets/texture/gameover.png"
        );
    }
    //入力遅延タイマーを初期化
    m_inputDelayTimer = 0.0f;
}

/**
 * @brief クリアシーンの終了処理
 *
 * シーンが破棄される前に呼ばれるリソース解放処理。
 */
void GameOverScene::dispose()
{
}

REGISTER_CLASS(GameOverScene);