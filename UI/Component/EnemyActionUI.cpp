#include "EnemyActionUI.h"
#include "../../Core/GameContext.h"
#include "../../System/Camera.h"
#include "../../System/Utility/WorldToScreen.h"
#include "../../Core/Application.h"
#include <cmath>
#include "../../System/commontypes.h" //for pi

namespace {
    // --- アニメーション定数 ---
    constexpr float ENTRANCE_DURATION = 0.5f;
    constexpr float SHAKE_DURATION = 0.5f;
    constexpr float SHAKE_INTERVAL = 10.0f;

    constexpr float ENTRANCE_BOUNCE_INTENSITY = 0.4f; // 登場時の最大拡大率の加算分
    constexpr float SHAKE_ANGLE_RAD = 0.3f; // 揺れの最大角度（約17度）
    constexpr float SHAKE_SPEED_MULTIPLIER = 6.0f; // 揺れの速度（往復回数に影響）

    // --- 配置定数 ---
    constexpr float WORLD_OFFSET_Y = 1.4f; // HPバーの少し上に配置
    constexpr float WORLD_OFFSET_X = 0.1f;
    constexpr float CULLING_MARGIN = 50.0f;
}

void EnemyActionUI::Init(GameContext* context) {
    m_context = context;

    for (int i = 1; i < 10; i++) {
        std::string dirPath = "Assets/texture/ui/ui_num_";
        std::string num = std::to_string(i);
        std::string fullPath = dirPath + num + ".png";
        m_numSprites.push_back(std::make_unique<CSprite>(48, 48, fullPath));
    }

    m_state = AnimState::Entrance;
    m_animTimer = 0.0f;
    m_intervalTimer = 0.0f;
}

void EnemyActionUI::Update(float dt) {
    m_animTimer += dt;

    switch (m_state) {
    case AnimState::Entrance:
        // 登場アニメーション：0 -> 1.2 -> 1.0 (バウンス効果)
        if (m_animTimer < ENTRANCE_DURATION) {
            float t = m_animTimer / ENTRANCE_DURATION;
            // 簡易的な BackOut 補間: 一旦大きく表示してから 1.0 に戻る曲線
            float s = sinf(t * PI);
            m_currentScale = t + (s * ENTRANCE_BOUNCE_INTENSITY);// 0 -> 約1.4 -> 1.0
            m_currentRotationZ = 0.0f;
        }
        else {
            m_currentScale = 1.0f;
            m_state = AnimState::Idle;
            m_animTimer = 0.0f;
            m_intervalTimer = 0.0f; // 周期タイマーをリセット
        }
        break;

    case AnimState::Idle:
        m_currentScale = 1.0f;
        m_currentRotationZ = 0.0f;

        // 周期実行用タイマーの累積
        m_intervalTimer += dt;
        if (m_intervalTimer >= SHAKE_INTERVAL) {
            m_state = AnimState::Shake;
            m_animTimer = 0.0f;
            m_intervalTimer = 0.0f;
        }
        break;

    case AnimState::Shake:
        // 揺れアニメーション：左右にシェイク
        if (m_animTimer < SHAKE_DURATION) {
            float t = m_animTimer / SHAKE_DURATION;
            // 正弦波（Sine wave）による揺れ：左右に3回往復
            float angle = sinf(t * PI * SHAKE_SPEED_MULTIPLIER) * SHAKE_ANGLE_RAD;
            m_currentRotationZ = angle;
            m_currentScale = 1.0f;
        }
        else {
            m_state = AnimState::Idle;
            m_animTimer = 0.0f;
            m_currentRotationZ = 0.0f;
        }
        break;
    }
}

void EnemyActionUI::Draw(const Vector3& worldPos, int order) {
    if (!m_context || order < 1 || order > static_cast<int>(m_numSprites.size())) return;
     
    Camera* camera = m_context->GetCamera();
    if (!camera) return;

    Vector3 uiPos = worldPos;
    uiPos.y += WORLD_OFFSET_Y;
    uiPos.x += WORLD_OFFSET_X;

    float screenW = (float)Application::GetWidth();
    float screenH = (float)Application::GetHeight();
    Vector2 screenPos = WorldToScreen(uiPos, camera->GetViewMatrix(), camera->GetProjMatrix(), screenW, screenH);

    if (screenPos.x < -CULLING_MARGIN || screenPos.x > screenW + CULLING_MARGIN ||
        screenPos.y < -CULLING_MARGIN || screenPos.y > screenH + CULLING_MARGIN) {
        return;
    }

    // 対応するSpriteを取得（orderは1から始まるため、インデックス用に -1 する）
    int spriteIndex = order - 1;
    if (!m_numSprites[spriteIndex]) return;

    // Z軸のみを回転させて平面上での揺れを表現
    Vector3 scaleVec(m_currentScale, m_currentScale, 1.0f);
    Vector3 rotVec(0.0f, 0.0f, m_currentRotationZ);
    Vector3 drawPos(screenPos.x, screenPos.y, 0.0f);

    m_numSprites[spriteIndex]->Draw(scaleVec, rotVec, drawPos);
}