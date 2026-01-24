#include "DialogueUI.h"
#include "../manager/GameContext.h"
#include "../system/camera.h"
#include "../utility/WorldToScreen.h"
#include "../Application.h"

void DialogueUI::Init(GameContext* context)
{
    //context初期化
    m_context = context;
	//「吹き出し」スパライトをロード
	m_dialogueSprite = std::make_unique<CSprite>(370 , 112, "assets/texture/ui/ui_dialogue_help.png");
    //初期は見えないように
    m_isVisible = false;
}

void DialogueUI::ShowDialogue(const Vector3& targetWorldPos, float duration)
{
    m_targetPos = targetWorldPos;//外部のUnitの位置
    m_targetPos.y += 1.7f; // 頭の上に表示
    m_timer = duration;//timerを設定
    m_isVisible = true;//可視化
}

void DialogueUI::Update(uint64_t dt)
{   //Updateの前提検査
    if (!m_isVisible) return;
    //ミリ秒を秒に
    float deltaSeconds = static_cast<float>(dt) / 1000.0f;
    //updateごとに表示残る時間を消す
    m_timer -= deltaSeconds;
    //表示時間が０以下になたら、「吹き出し」を表示しないのフラグ設置
    //次のUpdateの時、前提検査によって、Update実行しない
    if (m_timer <= 0.0f)
    {
        m_isVisible = false;
    }
}

void DialogueUI::Draw()
{
    //表示のフラグ　TRUE と　スパライトあり　と　Contextあり
    if (!m_isVisible || !m_dialogueSprite || !m_context) return;
    //Contextからカメラ所得、保存
    //３Ｄ空間のUnit位置で2Ｄのスパライト表示するため
    Camera* cam = m_context->GetCamera();
    //エラー防止：カメラあるか
    if (!cam) return;
    //スクリーンの範囲所得
    float sw = (float)Application::GetWidth();
    float sh = (float)Application::GetHeight();

    // 「吹き出し」表示位置のワールド座標->スクリーン座標の変換
    Vector2 screenPos = WorldToScreen(m_targetPos, cam->GetViewMatrix(), cam->GetProjMatrix(), sw, sh);

    // 簡単なスクリーン外カーリング
    if (screenPos.x < -50 || screenPos.x > sw + 50 || screenPos.y < -50 || screenPos.y > sh + 50) return;


    //描画設定：Zーバッファ閉じる、常に最上層に描画されるよう
    //透過部分を正しく描写するため、アルファブレンディング（透過合成）を有効化。
    Renderer::SetDepthEnable(false);
    Renderer::SetBlendState(BS_ALPHABLEND);

    // スクリーン正式描画
    m_dialogueSprite->Draw(Vector3(0.8f, 0.8f, 0.8f), Vector3(0, 0, 0), Vector3(screenPos.x, screenPos.y, 0));

    Renderer::SetBlendState(BS_NONE);
    Renderer::SetDepthEnable(true);
}