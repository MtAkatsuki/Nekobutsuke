#include "../main.h"
#include "titlescene.h"
#include "../system/CDirectInput.h"
#include "../system/scenemanager.h"
#include "../system/SceneClassFactory.h"
#include "../manager/AudioManager.h"
/**
 * @brief タイトルシーンのコンストラクタ
 *
 * 
 */
TitleScene::TitleScene()
{
}

/**
 * @brief タイトルシーンの更新処理
 *
 * Enterキー（リターンキー）が押されたら、`GameMainScene` へフェードアウト遷移する。
 *
 * @param deltatime 前フレームからの経過時間（マイクロ秒）
 */
void TitleScene::update(uint64_t deltatime)
{
    float deltaSeconds = static_cast<float>(deltatime) / 1000.0f;

    // m_hintSprite透明度変化コントロール用
    m_blinkTimer += deltaSeconds;

    // --- 任意キー入力のチェック ---
    bool anyKeyPressed = false;
    // 1から255までの主要なキーコードをすべてチェックする
    for (int i = 1; i < 256; i++)
    {
        if (CDirectInput::GetInstance().CheckKeyBufferTrigger(i))
        {
            anyKeyPressed = true;
            break; // いずれかのキーが見つかったらループを抜ける
        }
    }

    // 任意のキーが押されたらシーンを遷移させる
    if (anyKeyPressed)
    {
        SceneManager::GetInstance().SetCurrentScene(
            "GameScene",
            std::make_unique<FadeTransition>(1000.0f, FadeTransition::Mode::FadeInOut)
        );
    }
}

/**
 * @brief タイトルシーンの描画処理
 *
 * タイトル画像を画面中央に描画する。
 *
 * @param deltatime 前フレームからの経過時間（マイクロ秒）
 */
void TitleScene::draw(uint64_t deltatime)
{
    m_image->Draw(
        Vector3(1.0f, 1.0f, 1.0f),
        Vector3(0.0f, 0.0f, 0.0f),
        Vector3(SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f, 0.0f)
    );
    //呼吸アニメーション
    if (m_hintSprite)
    {

        // --- 透明度の計算 (サイン波による明滅エフェクト) ---
            // sin関数（-1.0～1.0）をabsで絶対値化（0.0～1.0）し、
            // 係数3.0fで点滅スピードを調整。
        float alpha = std::abs(std::sin(m_blinkTimer * 1.0f));

        // 完全に消えないよう、透明度の範囲を 0.3 ～ 1.0 にクランプ
        alpha = 0.3f + (0.7f * alpha);

        // --- マテリアルの更新 (アルファ値を適用) ---
        MATERIAL mtrl;
        mtrl.Diffuse = Color(1.0f, 1.0f, 1.0f, alpha);
        mtrl.TextureEnable = TRUE;
        m_hintSprite->ModifyMtrl(mtrl);

        // --- 座標計算 (画面下部の中央に配置) ---
        float posX = SCREEN_WIDTH / 2.0f;
        float posY = SCREEN_HEIGHT * 0.8f;

        // --- レンダリング設定 (透過・重なり順の制御) ---
        // アルファブレンディングを有効にし、深度テストを無効化して最前面に描画
        Renderer::SetBlendState(BS_ALPHABLEND);
        Renderer::SetDepthEnable(false);

        m_hintSprite->Draw(Vector3(1, 1, 1), Vector3(0, 0, 0), Vector3(posX, posY, 0));

        // ステートのリセット
        Renderer::SetDepthEnable(true);
        Renderer::SetBlendState(BS_NONE);
    }
}

/**
 * @brief タイトルシーンの初期化処理
 *
 * シーンがアクティブになる際に一度だけ呼ばれる初期化処理。
 */
void TitleScene::Init()
{
    static bool first = true;
    if (first) {

        m_image = std::make_unique<CSprite>(
            SCREEN_WIDTH,
            SCREEN_HEIGHT,
            "assets/texture/title.png"
        );
        m_hintSprite = std::make_unique<CSprite>(677.0f, 369.0f, "assets/texture/ui/ui_press_enter.png");
    }

    m_blinkTimer = 0.0f;

    // タイトルBGMの再生
    // loop = true (通常、タイトル曲はループ再生させる)
    // fadeTime = 1.0f (1秒かけてフェードイン)
    AudioManager::GetInstance().PlayBGM("Title", true, 1.0f);
}

/**
 * @brief タイトルシーンの終了処理
 *
 * シーンが破棄される前に呼ばれるリソース解放処理。
 */
void TitleScene::dispose()
{
}

REGISTER_CLASS(TitleScene);