#include "EnemyActionUI.h"
#include "../manager/GameContext.h"
#include "../system/camera.h"
#include "../utility/WorldToScreen.h"
#include "../Application.h"
#include <cmath>

void EnemyActionUI::Init(GameContext* context) {
    m_context = context;

    // 数字 1-3 のテクスチャをロード
    // 画像サイズは 40x40 程度を想定
    m_numSprites.push_back(std::make_unique<CSprite>(48, 48, "assets/texture/ui/ui_num_1.png"));
    m_numSprites.push_back(std::make_unique<CSprite>(48, 48, "assets/texture/ui/ui_num_2.png"));
    m_numSprites.push_back(std::make_unique<CSprite>(48, 48, "assets/texture/ui/ui_num_3.png"));

    // 初期状態の設定
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
            float s = sinf(t * 3.14159f);
            m_currentScale = t + (s * 0.4f); // 0 -> 約1.4 -> 1.0
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
            float angle = sinf(t * 3.14159f * 6.0f) * 0.3f; // 0.3ラジアン（約17度）
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
    // 安全チェック：行動順が3位以内か、または無効なコンテキストの場合は描画しない
    if (!m_context || order < 1 || order > 3) return;
    if (order > (int)m_numSprites.size()) return;

    Camera* camera = m_context->GetCamera();
    if (!camera) return;

    // スクリーン座標の計算
    // 数字をHPバーの上方に表示（HPバーのオフセット y+1.1f を考慮し y+1.6f に設定）
    Vector3 uiPos = worldPos;
    uiPos.y += 1.2f;
    uiPos.x += 0.1f;

    float screenW = (float)Application::GetWidth();
    float screenH = (float)Application::GetHeight();
    Vector2 screenPos = WorldToScreen(uiPos, camera->GetViewMatrix(), camera->GetProjMatrix(), screenW, screenH);

    // 画面外クリッピング（カリング）
    if (screenPos.x < -50 || screenPos.x > screenW + 50 || screenPos.y < -50 || screenPos.y > screenH + 50) return;

    // 対応するSpriteを取得（orderは1から始まるため、インデックス用に -1 する）
    int spriteIndex = order - 1;
    if (!m_numSprites[spriteIndex]) return;

    // アニメーションによるトランスフォームの適用
    // CSprite::Draw(scale, rotation, position)
    // Z軸のみを回転させて平面上での揺れを表現
    Vector3 scaleVec(m_currentScale, m_currentScale, 1.0f);
    Vector3 rotVec(0.0f, 0.0f, m_currentRotationZ);
    Vector3 drawPos(screenPos.x, screenPos.y, 0.0f);

    m_numSprites[spriteIndex]->Draw(scaleVec, rotVec, drawPos);
}