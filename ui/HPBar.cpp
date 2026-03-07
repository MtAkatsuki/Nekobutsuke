#include "HPBar.h"
#include "../manager/GameContext.h"
#include "../system/camera.h"
#include "../utility/WorldToScreen.h"
#include "../Application.h"
#include <algorithm> // for max

float HPBar::s_hpBarOffsetY = 1.0f;
float HPBar::s_hpBarTexSize = 22.0f;
float HPBar::s_hpBarGap = 1.0f;

void HPBar::Init(GameContext* context)
{
    m_context = context;

    // 元のブロックサイズ
    m_frameSprite = std::make_unique<CSprite>(100, 84, "assets/texture/UI/ui_heart_frame.png");
    m_fullSprite = std::make_unique<CSprite>(78, 56, "assets/texture/UI/ui_heart_full.png");

    m_texWidth = 100.0f;
    m_texHeight = 84.0f;
}

void HPBar::Update(float dt) {
    m_blinkTimer += dt;
    if (m_blinkTimer >= 0.4f) { //0.2秒ごとに状態を切り替える
        m_blinkState = !m_blinkState;
        m_blinkTimer = 0.0f;
    }
}

void HPBar::Draw(const Vector3& worldPos, int currentHP, int maxHP, int previewDamage)
{
    if (!m_context || maxHP <= 0 || currentHP <= 0) return;

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


    // ==========================================
    //  等比スケーリングの計算
    //  ImGuiで設定された s_hpBarTexSize を目標の幅とする
    //  アスペクト比を維持するため、算出されたスケール値をXとYの両方に適用する
     // ==========================================
    float scaleRate = s_hpBarTexSize / m_texWidth;
    Vector3 scaleVec(scaleRate, scaleRate, 1.0f);

    // 実際に画面へ描画される枠の幅（間隔計算に使用）
    float actualFrameWidth = m_texWidth * scaleRate;

    // UI全体の幅を算出し、中央揃えにするための開始座標を決定
    float totalWidth = maxHP * actualFrameWidth + (maxHP - 1) * s_hpBarGap;
    float startX = screenPos.x - totalWidth / 2.0f + actualFrameWidth / 2.0f;
    float drawY = screenPos.y;

    // 【HPゼロ時は表示しない】という仕様に基づき、currentHP分だけループして描画
    for (int i = 0; i < currentHP; i++)
    {
        float x = startX + i * (actualFrameWidth + s_hpBarGap);
        Vector3 pos(x, drawY, 0);

        // 1. 常に底枠（空のハート）を描画
        m_frameSprite->Draw(scaleVec, Vector3(0, 0, 0), pos);

        // 2. そのハートがダメージプレビュー範囲内（右側優先）にあるか判定
        bool isPreviewing = (i >= currentHP - previewDamage);

        // プレビュー対象外、または点滅の「点灯」周期であれば赤心を描画
        if (!isPreviewing || m_blinkState) {
            m_fullSprite->Draw(scaleVec, Vector3(0, 0, 0), pos);
        }
    }
}
