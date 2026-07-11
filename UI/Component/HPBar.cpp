#include "HPBar.h"
#include "../../Core/GameContext.h"
#include "../../System/Camera.h"
#include "../../System/Utility/WorldToScreen.h"
#include "../../Core/Application.h"
#include <algorithm> // for max

namespace {
    // --- 定数定義 (Magic Numbers) ---
    constexpr float BLINK_INTERVAL_SEC = 0.4f; // 点滅周期（秒）
    constexpr float BASE_TEX_WIDTH = 100.0f;   // 枠テクスチャの実寸幅（px）
    constexpr float BASE_TEX_HEIGHT = 84.0f;   // 枠テクスチャの実寸高さ（px）
    constexpr float FULL_TEX_WIDTH = 78.0f;    // ハート中身テクスチャの実寸幅（px）
    constexpr float FULL_TEX_HEIGHT = 56.0f;   // ハート中身テクスチャの実寸高さ（px）
    constexpr float CULLING_MARGIN = 100.0f; // 画面外カリングの余裕マージン
    constexpr float WORLD_OFFSET_Y = 1.1f;   // ユニットの足元から頭上へのオフセット
}

float HPBar::s_hpBarOffsetY = 1.0f;
float HPBar::s_hpBarTexSize = 22.0f;
float HPBar::s_hpBarGap = 1.0f;

void HPBar::Init(GameContext* context) {
    m_context = context;

    m_frameSprite = std::make_unique<CSprite>(BASE_TEX_WIDTH, BASE_TEX_HEIGHT, "Assets/texture/UI/ui_heart_frame.png");
    m_fullSprite = std::make_unique<CSprite>(FULL_TEX_WIDTH, FULL_TEX_HEIGHT, "Assets/texture/UI/ui_heart_full.png");
}

void HPBar::Update(float dt) {
    m_blinkTimer += dt;
    // ダメージプレビュー用の点滅状態を一定周期でトグル
    if (m_blinkTimer >= BLINK_INTERVAL_SEC) {
        m_isBlinkOn = !m_isBlinkOn;
        m_blinkTimer = 0.0f;
    }
}

void HPBar::Draw(const Vector3& worldPos, int currentHP, int maxHP, int previewDamage){
    if (!m_context || maxHP <= 0 || currentHP <= 0) return;

    Camera* camera = m_context->GetCamera();
    if (!camera) return;

    // 座標変換(3D -> 2D)
    Vector3 drawPos = worldPos;
    drawPos.y += WORLD_OFFSET_Y;

    float screenW = (float)Application::GetWidth();
    float screenH = (float)Application::GetHeight();
    Vector2 screenPos = WorldToScreen(drawPos, camera->GetViewMatrix(), camera->GetProjMatrix(), screenW, screenH);

    // 画面外カリング (パフォーマンス最適化)
    if (screenPos.x < -CULLING_MARGIN || screenPos.x > screenW + CULLING_MARGIN ||
        screenPos.y < -CULLING_MARGIN || screenPos.y > screenH + CULLING_MARGIN) {
        return;
    }

    // 動的スケーリング計算
    
    // ImGuiの設定値(s_hpBarTexSize)を基準にアスペクト比を維持
    float scaleRate = s_hpBarTexSize / BASE_TEX_WIDTH;
    Vector3 scaleVec(scaleRate, scaleRate, 1.0f);
    float actualFrameWidth = BASE_TEX_WIDTH * scaleRate;

    // UI全体を中央揃えにするための開始X座標を算出
    float totalWidth = (maxHP * actualFrameWidth) + ((maxHP - 1) * s_hpBarGap);
    float startX = screenPos.x - (totalWidth / 2.0f) + (actualFrameWidth / 2.0f);
    float drawY = screenPos.y;

    // HPゼロ時は非表示とするため、currentHPの数だけ描画ループ
    for (int i = 0; i < currentHP; ++i) {
        float x = startX + (i * (actualFrameWidth + s_hpBarGap));
        Vector3 pos(x, drawY, 0.0f);

        // 1. 常に底枠を描画
        m_frameSprite->Draw(scaleVec, Vector3(0, 0, 0), pos);

        // 2. ダメージプレビュー範囲内か判定（右側から減る想定）
        bool isPreviewingTarget = (i >= currentHP - previewDamage);

        // プレビュー対象外、または点滅の「点灯」タイミングであれば中身を描画
        if (!isPreviewingTarget || m_isBlinkOn) {
            m_fullSprite->Draw(scaleVec, Vector3(0, 0, 0), pos);
        }
    }
}
