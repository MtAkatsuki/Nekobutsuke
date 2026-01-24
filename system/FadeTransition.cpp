#include    "../main.h"
#include    "../system/scenemanager.h"
#include    "FadeTransition.h"
#include    "BoxDrawer.h"

/**
 * @brief フェード遷移の開始処理
 *
 * モードに応じてフェーズを設定し、必要に応じて先にシーンを切り替える。
 *
 * @param nextSceneName 遷移先のシーン名
 */
void FadeTransition::start() {
    m_elapsed = 0;
    m_alpha = 0.0f;

    switch (m_mode) {
    case Mode::FadeOutOnly://黒になるだけ：ー＞黒幕
        m_alpha = 0.0f;
        m_phase = Phase::FadeOut;
        break;
    case Mode::FadeInOut://ー＞黒幕ー＞シーン変更ー＞透明
        m_alpha = 0.0f;
        m_phase = Phase::FadeOut;
        break;
    case Mode::FadeInOnly://明るくなるだけ：黒幕(WAIT)ー＞シーン変更
        m_alpha = 1.0f;
        m_phase = Phase::Wait;
        break;

    }
}

/**
 * @brief フェードの進行を更新する
 *
 * 経過時間に基づいて透明度（m_alpha）を変化させる。
 * フェードアウト終了後はシーンを切り替え、フェードインに移行する。
 *
 * @param deltaTime 前フレームからの経過時間（マイクロ秒）
 */
void FadeTransition::update(uint64_t deltaTime) {
    m_elapsed += deltaTime;

    switch (m_phase) {
    case Phase::FadeOut:
        m_alpha = static_cast<float>(m_elapsed) / m_duration;
        if (m_elapsed >= m_duration) {
            m_alpha = 1.0f;
            m_elapsed = 0;

            if (m_mode == Mode::FadeOutOnly) {
                m_phase = Phase::Idle;
            }
            else {
                m_phase = Phase::Wait;
            }
        }
        break;

    case Phase::FadeIn:
        m_alpha = 1.0f - static_cast<float>(m_elapsed) / m_duration;
        if (m_elapsed >= m_duration) {//fade inアニメション終わり
            m_alpha = 0.0f;//最終なalpha値になる
            m_phase = Phase::Idle;
        }
        break;
    case Phase::Wait:
        m_alpha = 1.0f;//黒で待つ
        break;

    default:
        break;
    }
}

void FadeTransition::onSceneSwapped() 
{
    //シーン遷移完了、モード必要となると、FadeIn始まる
    if (m_mode == Mode::FadeInOnly || m_mode == Mode::FadeInOut) {
        m_phase = Phase::FadeIn;
        m_elapsed = 0;
    }
    else { 
        m_phase = Phase::Idle; }
}

/**
 * @brief フェード用の黒い矩形を画面に描画する
 *
 * 現在の透明度に応じた黒い全画面オーバーレイを表示し、自然なフェード効果を演出する。
 */
void FadeTransition::draw() {
    if (m_phase != Phase::Idle) {
        //マスクは常に一番上にセットする
        Renderer::SetDepthEnable(false);

        //1.単位行列をセット(カメラの位置を基本状態に戻る)
        Matrix4x4 view = Matrix4x4::Identity;
        Renderer::SetViewMatrix(&view);
        //2.直投影セットする
        //左上:(0,0);右下(Width, Height)
        Matrix4x4 proj = Matrix4x4::CreateOrthographicOffCenter
        (0.0f, (float)SCREEN_WIDTH,//左 -> 右
        (float)SCREEN_HEIGHT, 0.0f,// 下 -> 上
         0.0f, 1.0f                //近いー＞遠い
        );
        Renderer::SetProjectionMatrix(&proj);
        //3.描画
        BoxDrawerDraw(
            (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT, 1.0f,
            Color(0, 0, 0, m_alpha),
            SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f, 0.0f
        );
        Renderer::SetDepthEnable(true);
    }
}

/**
 * @brief フェード演出が完了したかを判定する
 *
 * 現在のフェーズが None になっていれば演出完了。
 *
 * @return true フェードが完了している
 * @return false まだ演出中である
 */
bool FadeTransition::isFinished() const {
    return m_phase == Phase::Idle;
}
