#include "DialogueUI.h"
#include "../../Core/GameContext.h"
#include "../../System/Camera.h"
#include "../../System/Utility/WorldToScreen.h"
#include "../../Core/Application.h"

namespace {
    // --- 定数定義 ---
    constexpr float WORLD_OFFSET_Y = 1.5f; // 頭上に表示するためのY軸オフセット
    constexpr float CULLING_MARGIN = 50.0f;
    constexpr float DEFAULT_TINT = 0.8f; // スプライトの標準乗算カラー

    // --- 吹き出しスプライトのテクスチャ実寸（px） ---
    constexpr float HELP_TEX_W = 370.0f;
    constexpr float HELP_TEX_H = 112.0f;
    constexpr float ESCAPE_TEX_W = 517.0f;
    constexpr float ESCAPE_TEX_H = 156.0f;
}

void DialogueUI::Init(GameContext* context) {
    m_context = context;

    m_dialogueHelpSprite = std::make_unique<CSprite>(HELP_TEX_W, HELP_TEX_H, "Assets/texture/ui/ui_dialogue_help.png");
    m_dialogueEscapeSprite = std::make_unique<CSprite>(ESCAPE_TEX_W, ESCAPE_TEX_H, "Assets/texture/ui/ui_dialogue_escape.png");

    m_isVisible = false;
}

void DialogueUI::Update(float deltaSeconds) {
    if (!m_isVisible) return;

    // タイマーが正数の場合のみ時間を減算（負数は無限表示扱い）
    if (m_displayTimer > 0.0f) {
        m_displayTimer -= deltaSeconds;

        if (m_displayTimer <= 0.0f) {
            HideDialogue();
        }
    }
}

void DialogueUI::Draw() {
    if (!m_isVisible || !m_currentSprite || !m_context) return;

    Camera* cam = m_context->GetCamera();
    if (!cam) return;

    float sw = (float)Application::GetWidth();
    float sh = (float)Application::GetHeight();

    Vector2 screenPos = WorldToScreen(m_targetPos, cam->GetViewMatrix(), cam->GetProjMatrix(), sw, sh);

    if (screenPos.x < -CULLING_MARGIN || screenPos.x > sw + CULLING_MARGIN ||
        screenPos.y < -CULLING_MARGIN || screenPos.y > sh + CULLING_MARGIN) {
        return;
    }

    // 透過部分を正しく描写するため、Zバッファを無効化しアルファブレンドを有効化
    Renderer::SetDepthEnable(false);
    Renderer::SetBlendState(BS_ALPHABLEND);

    Vector3 colorTint(DEFAULT_TINT, DEFAULT_TINT, DEFAULT_TINT);
    m_currentSprite->Draw(colorTint, Vector3(0, 0, 0), Vector3(screenPos.x, screenPos.y, 0));

    Renderer::SetBlendState(BS_NONE);
    Renderer::SetDepthEnable(true);
}

void DialogueUI::ShowDialogue(const Vector3& targetWorldPos, DialogueType type, float duration)
{
    m_targetPos = targetWorldPos;
    m_targetPos.y += WORLD_OFFSET_Y; // 頭の上に表示

    if (type == DialogueType::Escape) {
        m_currentSprite = m_dialogueEscapeSprite.get();
    }
    else {
        m_currentSprite = m_dialogueHelpSprite.get();
    }

    m_displayTimer = duration;
    m_isVisible = true;
}

void DialogueUI::HideDialogue(){
    m_isVisible = false;
}

