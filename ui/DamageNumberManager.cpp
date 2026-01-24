#include "DamageNumberManager.h"
#include "../utility/WorldToScreen.h"
#include "../Application.h"
#include "../manager/GameContext.h"
#include "../system/Camera.h"
#include <cmath>
#include <iostream>

void DamageNumberManager::Init(GameContext* context) {
    m_context = context;

	m_sprite =std::make_unique<CSprite>(16, 32, "assets/texture/suuji16x32_05.png");

    const char numberChars[] = "0123456789+-.";
    for (int i = 0; i < 13; ++i) {
        float u_start = static_cast<float>(i) / 13.0f;
        float u_end = static_cast<float>(i + 1) / 13.0f;
        UVQuad quad = {
            Vector2(u_start, 0.0f), Vector2(u_end, 0.0f),
            Vector2(u_start, 1.0f), Vector2(u_end, 1.0f)
        };
        m_numberUVs[numberChars[i]] = quad;
    }
}

void DamageNumberManager::SpawnDamage(Vector3 position, int damage) {
    ActiveDamageNumber dmg;
    dmg.startWorldPos = position;
    dmg.currentOffset = Vector3(0, 0, 0);
    dmg.number = damage;
    dmg.timer = 0.0f;

    dmg.timePhase1 = 0.1f;
    dmg.timePhase2 = 0.2f;
    dmg.totalTime = 0.3f;

    m_activeNumbers.push_back(dmg);
}

void DamageNumberManager::Update(uint64_t dt) {
    float deltaSeconds = static_cast<float>(dt) / 1000.0f;

    for (auto it = m_activeNumbers.begin(); it!=m_activeNumbers.end();) {
        it->timer += deltaSeconds;
        it->currentOffset.y += FLOAT_SPEED * deltaSeconds;

        if (it->timer >= it->totalTime) {
            it = m_activeNumbers.erase(it);
        }
        else { ++it; }
    }

}

void DamageNumberManager::Draw() {
    if (m_activeNumbers.empty() || !m_sprite) return;

    if (!m_context) {return;}

    Camera* currentCamera = m_context->GetCamera();
    if (!currentCamera) {return;}

    Matrix4x4 view = currentCamera->GetViewMatrix();
    Matrix4x4 proj = currentCamera->GetProjMatrix();
    float screenW = (float)Application::GetWidth();
    float screenH = (float)Application::GetHeight();
    //手動カリング用にカメラの位置と向きを取得
    Vector3 camPos = currentCamera->GetPosition();
    Vector3 camForward = currentCamera->GetLookat() - camPos;
    camForward.Normalize();

    for (const auto& dmg : m_activeNumbers) {

        float currentScale = 1.0f;
        float currentAlpha = 1.0f;

        // Phase 1: Fade In (EaseOut)
        if (dmg.timer < dmg.timePhase1) {
            float t = dmg.timer / dmg.timePhase1;
            t = 1.0f - pow(1.0f - t, 2.0f); // EaseOut Quad
            currentScale = std::lerp(0.0f, 1.0f, t);
            currentAlpha = std::lerp(0.0f, 1.0f, t);
        }
        // Phase 2: Wait
        else if (dmg.timer < dmg.timePhase2) {
            currentScale = 1.0f;
            currentAlpha = 1.0f;
        }
        // Phase 3: Fade Out (EaseIn)
        else {
            float fadeOutDuration = dmg.totalTime - dmg.timePhase2;
            float t = (dmg.timer - dmg.timePhase2) / fadeOutDuration;
            t = pow(t, 2.0f); // EaseIn Quad
            currentScale = std::lerp(1.0f, 0.0f, t);
            currentAlpha = std::lerp(1.0f, 0.0f, t);
        }

        Vector3 worldPos = dmg.startWorldPos + dmg.currentOffset;
        //点からカメラのベクトルとカメラ向きのドット積、もし0.2以下になたら、後ろに判定する
        Vector3 toPoint = worldPos - camPos;
        float dot = camForward.Dot(toPoint);

        if (dot < 0.2f) continue;

        Vector2 screenPos =WorldToScreen(worldPos, view, proj, screenW, screenH);
        //画面外削除
        if (screenPos.x < -50 || screenPos.x > screenW + 50 ||
            screenPos.y < -50 || screenPos.y > screenH + 50) continue;

        MATERIAL mtrl;
        mtrl.Ambient = Color(1.0f, 1.0f, 1.0f, 1.0f);
        mtrl.Diffuse = Color(1.0f, 1.0f, 1.0f, currentAlpha);
        mtrl.Specular = Color(0.0f, 0.0f, 0.0f, 0.0f);
        mtrl.Emission = Color(0.0f, 0.0f, 0.0f, 0.0f);
        
        mtrl.TextureEnable = TRUE;
        m_sprite->ModifyMtrl(mtrl);

        std::string dmgNumStr = std::to_string(dmg.number);

        float totalWidth = (dmgNumStr.length() - 1) * CHAR_WIDTH * currentScale;
        float startX = screenPos.x - (totalWidth / 2.0f);

        Vector3 position = { startX,screenPos.y,0.0f };
        Vector3 drawScale = { currentScale,currentScale,1.0f };

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