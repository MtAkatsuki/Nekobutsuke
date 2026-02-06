#include "EffectManager.h"
#include "../manager/GameContext.h"
#include "../system/camera.h"
#include "../utility/WorldToScreen.h"
#include "../Application.h"

void EffectManager::Init(GameContext* context) {
    m_context = context;
    // 4種類のテクスチャをロード (ファイル名の整合性に注意)
    m_textures.push_back(std::make_unique<CSprite>(50, 46, "assets/texture/effect/rubble_01.png"));
    m_textures.push_back(std::make_unique<CSprite>(48, 46, "assets/texture/effect/rubble_02.png"));
    m_textures.push_back(std::make_unique<CSprite>(51, 41, "assets/texture/effect/rubble_03.png"));
    m_textures.push_back(std::make_unique<CSprite>(42, 50, "assets/texture/effect/rubble_04.png"));

    std::random_device rd;
    m_gen = std::mt19937(rd());
}

void EffectManager::SpawnRubble(const Vector3& worldPos, int count) {
    if (!m_context) return;
    Camera* cam = m_context->GetCamera();
    if (!cam) return;

    // 1. 3Dの足元座標をスクリーン座標に変換
    float sw = (float)Application::GetWidth();
    float sh = (float)Application::GetHeight();
    Vector2 screenPos = WorldToScreen(worldPos, cam->GetViewMatrix(), cam->GetProjMatrix(), sw, sh);

    // 画面外カリング (境界チェック)
    if (screenPos.x < -50 || screenPos.x > sw + 50 || screenPos.y < -50 || screenPos.y > sh + 50) return;

    // 2. パーティクルの生成
    std::uniform_real_distribution<float> distVelX(-150.0f, 150.0f); // 横方向の拡散
    std::uniform_real_distribution<float> distVelY(-400.0f, -200.0f); // 上方向への跳ね返り
    std::uniform_real_distribution<float> distScale(0.5f, 1.2f);
    std::uniform_int_distribution<int> distTex(0, 3);

    for (int i = 0; i < count; ++i) {
        RubbleParticle p;
        p.active = true;
        p.pos = screenPos;
        p.velocity = Vector2(distVelX(m_gen), distVelY(m_gen));
        p.rotation = 0.0f;
        p.rotSpeed = distVelX(m_gen) * 0.05f; // ランダムな自転速度
        p.scale = distScale(m_gen);
        p.lifeTime = 1.0f; // 寿命は1秒
        p.textureIndex = distTex(m_gen);

        m_particles.push_back(p);
    }
}

void EffectManager::Update(float dt) {
    const float GRAVITY = 1200.0f; // 重力加速度

    for (auto& p : m_particles) {
        if (!p.active) continue;

        p.lifeTime -= dt;
        if (p.lifeTime <= 0.0f) {
            p.active = false;
            continue;
        }

        // 物理更新
        p.velocity.y += GRAVITY * dt;
        p.pos.x += p.velocity.x * dt;
        p.pos.y += p.velocity.y * dt;
        p.rotation += p.rotSpeed * dt;
    }

    // 非アクティブなパーティクルを削除 (remove_if イディオム)
    m_particles.erase(std::remove_if(m_particles.begin(), m_particles.end(),
        [](const RubbleParticle& p) { return !p.active; }), m_particles.end());
}

void EffectManager::Draw() {
    for (const auto& p : m_particles) {
        if (p.active && p.textureIndex < m_textures.size()) {
            // 簡易的なフェードアウト (時間の経過に合わせてスケールを縮小)
            float fadeScale = p.scale * (p.lifeTime);

            m_textures[p.textureIndex]->Draw(
                Vector3(fadeScale, fadeScale, 1.0f),
                Vector3(0, 0, p.rotation),
                Vector3(p.pos.x, p.pos.y, 0)
            );
        }
    }
}

void EffectManager::Clear() {
    m_particles.clear();
}