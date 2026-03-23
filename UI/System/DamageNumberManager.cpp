#include "DamageNumberManager.h"
#include "../../System/Utility/WorldToScreen.h"
#include "../../Core/Application.h"
#include "../../Core/GameContext.h"
#include "../../System/Camera.h"
#include <cmath>
#include <iostream>

namespace {
    // --- 動作・描画定数 (Magic Numbers) ---
    constexpr float FLOAT_SPEED = 0.5f;  // 上昇速度
    constexpr float CHAR_WIDTH = 14.0f; // 1文字あたりの描画幅
    constexpr float CULLING_MARGIN = 50.0f;
    constexpr float DOT_PRODUCT_THRESHOLD = 0.2f;  // カメラ背面判定用の内積閾値

    // --- アニメーションフェーズ定数 ---
    constexpr float PHASE1_DURATION = 0.1f;
    constexpr float PHASE2_DURATION = 0.3f; // (絶対時間ではなく到達点としての設計維持)
    constexpr float TOTAL_DURATION = 0.5f;

    constexpr float MAX_SCALE_UP = 1.5f;

    // --- UV計算用 ---
    constexpr float UV_CELL_COUNT = 13.0f; // テクスチャ内の文字分割数
}

void DamageNumberManager::Init(GameContext* context) {
    m_context = context;
    m_sprite = std::make_unique<CSprite>(16, 32, "Assets/texture/suuji16x32_05.png");

    const char numberChars[] = "0123456789+-.";
    for (int i = 0; i < 13; ++i) {
        float u_start = static_cast<float>(i) / UV_CELL_COUNT;
        float u_end = static_cast<float>(i + 1) / UV_CELL_COUNT;

        UVQuad quad = {
            Vector2(u_start, 0.0f), Vector2(u_end, 0.0f),
            Vector2(u_start, 1.0f), Vector2(u_end, 1.0f)
        };
        m_numberUVs[numberChars[i]] = quad;
    }
}

void DamageNumberManager::Update(uint64_t dt) {
    float deltaSeconds = static_cast<float>(dt) / 1000.0f;
    // 寿命が尽きたダメージ表示の削除と上昇更新
    for (auto it = m_activeNumbers.begin(); it != m_activeNumbers.end();) {
        it->timer += deltaSeconds;
        it->currentOffset.y += FLOAT_SPEED * deltaSeconds;

        if (it->timer >= it->totalTime) {
            it = m_activeNumbers.erase(it);
        }
        else { ++it; }
    }

}

void DamageNumberManager::Draw() {
    if (m_activeNumbers.empty() || !m_sprite || !m_context) return;

    Camera* currentCamera = m_context->GetCamera();
    if (!currentCamera) return;

    Matrix4x4 view = currentCamera->GetViewMatrix();
    Matrix4x4 proj = currentCamera->GetProjMatrix();
    float screenW = static_cast<float>(Application::GetWidth());
    float screenH = static_cast<float>(Application::GetHeight());

    // 背面カリング用にカメラの方向ベクトルを計算
    Vector3 camPos = currentCamera->GetPosition();
    Vector3 camForward = currentCamera->GetLookat() - camPos;
    camForward.Normalize();

    for (const auto& dmg : m_activeNumbers) {
        float currentScale = 1.0f;
        float currentAlpha = 1.0f;

        // フェーズに基づく Ease アニメーション計算
        if (dmg.timer < dmg.timePhase1) {
            // Phase 1: Fade In (EaseOut Quad - 勢いよく飛び出す)
            float t = dmg.timer / dmg.timePhase1;
            t = 1.0f - std::pow(1.0f - t, 2.0f);
            currentScale = std::lerp(0.0f, MAX_SCALE_UP, t);
            currentAlpha = std::lerp(0.0f, 1.0f, t);
        }
        else if (dmg.timer < dmg.timePhase2) {
            // Phase 2: Wait (最大サイズで一時停止)
            currentScale = MAX_SCALE_UP;
            currentAlpha = 1.0f;
        }
        else {
            // Phase 3: Fade Out (EaseIn Quad - 加速しながら消える)
            float fadeOutDuration = dmg.totalTime - dmg.timePhase2;
            float t = (dmg.timer - dmg.timePhase2) / fadeOutDuration;
            t = std::pow(t, 2.0f);
            currentScale = std::lerp(MAX_SCALE_UP, 0.0f, t);
            currentAlpha = std::lerp(1.0f, 0.0f, t);
        }

        Vector3 worldPos = dmg.startWorldPos + dmg.currentOffset;

        // カメラの裏側に回った数値を描画しない（内積による簡易判定）
        Vector3 toPoint = worldPos - camPos;
        float dot = camForward.Dot(toPoint);
        if (dot < DOT_PRODUCT_THRESHOLD) continue;

        Vector2 screenPos = WorldToScreen(worldPos, view, proj, screenW, screenH);

        // 画面外カリング
        if (screenPos.x < -CULLING_MARGIN || screenPos.x > screenW + CULLING_MARGIN ||
            screenPos.y < -CULLING_MARGIN || screenPos.y > screenH + CULLING_MARGIN) {
            continue;
        }

        // マテリアルのアルファ更新
        MATERIAL mtrl;
        mtrl.Ambient = Color(1.0f, 1.0f, 1.0f, 1.0f);
        mtrl.Diffuse = Color(1.0f, 1.0f, 1.0f, currentAlpha);
        mtrl.Specular = Color(0.0f, 0.0f, 0.0f, 0.0f);
        mtrl.Emission = Color(0.0f, 0.0f, 0.0f, 0.0f);
        mtrl.TextureEnable = TRUE;
        m_sprite->ModifyMtrl(mtrl);

        std::string dmgNumStr = std::to_string(dmg.number);

        // 中央揃えのための計算
        float totalWidth = (dmgNumStr.length() - 1) * CHAR_WIDTH * currentScale;
        float startX = screenPos.x - (totalWidth / 2.0f);

        Vector3 position = { startX, screenPos.y, 0.0f };
        Vector3 drawScale = { currentScale, currentScale, 1.0f };

        // 1文字ずつUVを切り替えて描画
        for (char c : dmgNumStr) {
            if (m_numberUVs.count(c)) {
                const UVQuad& uv = m_numberUVs[c];
                m_sprite->ModifyUV(uv.tex);
                m_sprite->Draw(drawScale, Vector3(0, 0, 0), position);
            }
            position.x += CHAR_WIDTH * currentScale;
        }
    }
}

void DamageNumberManager::SpawnDamage(Vector3 position, int damage) {
    ActiveDamageNumber dmg;
    dmg.startWorldPos = position;
    dmg.currentOffset = Vector3(0, 0, 0);
    dmg.number = damage;
    dmg.timer = 0.0f;

    dmg.timePhase1 = PHASE1_DURATION;
    dmg.timePhase2 = PHASE2_DURATION;
    dmg.totalTime = TOTAL_DURATION;

    m_activeNumbers.push_back(dmg);
}