#include "HPBar.h"
#include "../manager/GameContext.h"
#include "../system/camera.h"
#include "../utility/WorldToScreen.h"
#include "../Application.h"
#include <algorithm> // for max

void HPBar::Init(GameContext* context)
{
    m_context = context;

    // アセット実際のサイズを入力する
    m_bgSprite = std::make_unique<CSprite>(DISPLAY_BG_WIDTH, DISPLAY_BG_HEIGHT, "assets/texture/UI/ui_hp_bar.png");

    // 元のブロックサイズ
    float rawBlockSize = 128.0f;
    m_greenSprite = std::make_unique<CSprite>(rawBlockSize, rawBlockSize, "assets/texture/UI/ui_hp_block_green.png");
    m_redSprite = std::make_unique<CSprite>(rawBlockSize, rawBlockSize, "assets/texture/UI/ui_hp_block_red.png");

    m_texBlockWidth = rawBlockSize;
    m_texBlockHeight = rawBlockSize;
}

void HPBar::Draw(const Vector3& worldPos, int currentHP, int maxHP, BarType type)
{
    if (!m_context || maxHP <= 0) return;

    Camera* camera = m_context->GetCamera();
    if (!camera) return;

    // 座標変換(3D -> 2D)
    Vector3 drawPos = worldPos;
	drawPos.y += 1.1f; // 高さ調整、ユニットの足元から少し上にオフセット

    float screenW = (float)Application::GetWidth();
    float screenH = (float)Application::GetHeight();
    Vector2 screenPos = WorldToScreen(drawPos, camera->GetViewMatrix(), camera->GetProjMatrix(), screenW, screenH);

    // 画面外カリング
    if (screenPos.x < -100 || screenPos.x > screenW + 100 || screenPos.y < -100 || screenPos.y > screenH + 100) return;


    // ベース板の描画
	// ベース板のスケール計算

        Vector3 bgScale(1.0f, 1.0f, 1.0f);
        m_bgSprite->Draw(bgScale, Vector3(0, 0, 0), Vector3(screenPos.x, screenPos.y, 0));


	// HPブロックの描画サイズ計算
	// 有効描画エリア＝総幅－左右パディング
    float contentWidth = DISPLAY_BG_WIDTH - (PADDING_X * 2);
	// ブロック間の隙間の数＝最大HP－1
    float totalGap = (maxHP > 1) ? (maxHP - 1) * GAP : 0.0f;
	// ブロックの目標幅 = (有効描画エリア - 総間隙) / 最大HP
    float targetBlockWidth = (contentWidth - totalGap) / (float)maxHP;
	// ボロックの目標高さ = ベース板の高さ - 上下パディング
    float targetBlockHeight = DISPLAY_BG_HEIGHT - (PADDING_Y * 2);

	// タイプに応じたスプライト選択
    CSprite* targetSprite = (type == BarType::Green) ? m_greenSprite.get() : m_redSprite.get();


        // スプライトのスケール計算
        //m_texBlockWidth->targetBlockWidth(目標幅)に合わせる
        float scaleX = targetBlockWidth / m_texBlockWidth;
        float scaleY = targetBlockHeight / m_texBlockHeight;
        Vector3 blockScale(scaleX, scaleY, 1.0f);

        // 現在HP分のブロックを描画
        // 計算開始 X (ベース板の左端 + パディング + ブロック幅の半分)
        // screenPos.xはベース板の中心位置
        // PADDING_Xはベース板の左端からブロック描画開始位置までの余白
        // targetBlockWidth / 2.0fはブロック中心合わせのため
        float startX = (screenPos.x - DISPLAY_BG_WIDTH / 2.0f) + PADDING_X + (targetBlockWidth / 2.0f);

        // 計算 Y (ベース板の中心に)
        float drawY = screenPos.y;

        for (int i = 0; i < currentHP; i++)
        {
            float x = startX + i * (targetBlockWidth + GAP);
            targetSprite->Draw(blockScale, Vector3(0, 0, 0), Vector3(x, drawY, 0));
        }
   
}