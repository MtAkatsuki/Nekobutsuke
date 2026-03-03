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

    //  ヒットエフェクト画像の読み込み (Index = 4)
    m_textures.push_back(std::make_unique<CSprite>(120, 120, "assets/texture/effect/hit_effect.png"));

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
        EffectParticle p; // [修正] 汎用的な EffectParticle を使用
        p.active = true;
        p.type = ParticleType::RUBBLE; // [新規追加] タイプフラグ
        p.pos = screenPos;
        p.velocity = Vector2(distVelX(m_gen), distVelY(m_gen));
        p.rotation = 0.0f;
        p.rotSpeed = distVelX(m_gen) * 0.05f; // ランダムな自転速度
        p.scale = distScale(m_gen);
        p.lifeTime = 1.0f; // 寿命は1秒
        p.maxLifeTime = 1.0f;
        p.textureIndex = distTex(m_gen);

        m_particles.push_back(p);
    }
}

// ヒットエフェクトの生成
void EffectManager::SpawnHitEffect(const Vector3& worldPos) {
    if (!m_context) return;
    Camera* cam = m_context->GetCamera();
    if (!cam) return;

    // 座標変換（ワールド座標からスクリーン座標へ）
    float sw = (float)Application::GetWidth();
    float sh = (float)Application::GetHeight();
    Vector2 screenPos = WorldToScreen(worldPos, cam->GetViewMatrix(), cam->GetProjMatrix(), sw, sh);

    // 画面外カリング（画面から大きく外れている場合は生成しない）
    if (screenPos.x < -100 || screenPos.x > sw + 100 || screenPos.y < -100 || screenPos.y > sh + 100) return;

    EffectParticle p;
    p.active = true;
    p.type = ParticleType::HIT_EFFECT; // ヒットエフェクトとして設定
    p.pos = screenPos;
    p.velocity = Vector2(0, 0); // 移動なし
    p.rotation = (float)(rand() % 360) * 3.14159f / 180.0f; // ランダムな初期角度
    p.rotSpeed = 0.0f;
    p.scale = 0.5f; // 初期サイズ
    p.maxLifeTime = 0.25f; // 持続時間は短めに設定
    p.lifeTime = p.maxLifeTime;
    p.textureIndex = 4; // hit_effect.png のインデックスを指定

    m_particles.push_back(p);
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

        // タイプに応じて異なる物理ロジックを実行
        if (p.type == ParticleType::RUBBLE) {
            // 瓦礫：重力の影響を受ける
            p.velocity.y += GRAVITY * dt;
            p.pos.x += p.velocity.x * dt;
            p.pos.y += p.velocity.y * dt;
            p.rotation += p.rotSpeed * dt;
        }
        else if (p.type == ParticleType::HIT_EFFECT) {
            // ヒットエフェクト：純粋なアニメーション（例：拡大してから縮小）
            float progress = 1.0f - (p.lifeTime / p.maxLifeTime); // 0.0 -> 1.0

            // シンプルなバウンス曲線：1.5倍まで素早く拡大し、その後少し戻る
            if (progress < 0.3f) {
                // 前半30%の時間：0.5 -> 1.5
                float t = progress / 0.3f;
                p.scale = 0.5f + t * 1.0f;
            }
            else {
                // 後半70%の時間：1.5 -> 1.0 (またはそれ以下)
                float t = (progress - 0.3f) / 0.7f;
                p.scale = 1.5f - t * 0.5f;
            }
        }
    }

    // 非アクティブなパーティクルを削除 (remove_if イディオム)
    m_particles.erase(std::remove_if(m_particles.begin(), m_particles.end(),
        [](const EffectParticle& p) { return !p.active; }), m_particles.end());
}

void EffectManager::DrawRubble() {
    for (const auto& p : m_particles) {
        if (p.active && p.type == ParticleType::RUBBLE && p.textureIndex < m_textures.size()) {
            float fadeAlpha = p.lifeTime / p.maxLifeTime;
            MATERIAL mtrl;
            mtrl.Diffuse = Color(1, 1, 1, fadeAlpha);
            mtrl.TextureEnable = true;
            m_textures[p.textureIndex]->ModifyMtrl(mtrl);
            float drawScale = p.scale * (p.lifeTime);
            m_textures[p.textureIndex]->Draw(
                Vector3(drawScale, drawScale, 1.0f), Vector3(0, 0, p.rotation), Vector3(p.pos.x, p.pos.y, 0)
            );
        }
    }
}

void EffectManager::DrawHitEffects() {
    for (const auto& p : m_particles) {
        if (p.active && p.type == ParticleType::HIT_EFFECT && p.textureIndex < m_textures.size()) {
            MATERIAL mtrl;
            mtrl.Diffuse = Color(1, 1, 1, 1.0f);
            mtrl.TextureEnable = true;
            m_textures[p.textureIndex]->ModifyMtrl(mtrl);
            m_textures[p.textureIndex]->Draw(
                Vector3(p.scale, p.scale, 1.0f), Vector3(0, 0, p.rotation), Vector3(p.pos.x, p.pos.y, 0)
            );
        }
    }
}

void EffectManager::Clear() {
    m_particles.clear();
}

void EffectManager::DrawStaticHitPreview(const Vector3& worldPos) {
    if (!m_context || !m_context->GetCamera()) return;

    Camera* cam = m_context->GetCamera();
    float sw = (float)Application::GetWidth();
    float sh = (float)Application::GetHeight();

    // スクリーン座標に変換
    Vector2 screenPos = WorldToScreen(worldPos, cam->GetViewMatrix(), cam->GetProjMatrix(), sw, sh);

    // 画面外のカリング（描画除外）処理
    if (screenPos.x < -100 || screenPos.x > sw + 100 || screenPos.y < -100 || screenPos.y > sh + 100) return;

    // ヒットエフェクト用テクスチャがロード済みか確認（hit_effect.png はインデックス 4）
    if (m_textures.size() > 4 && m_textures[4]) {

        MATERIAL mtrl;
        // 半透明の黄色に設定し、警告としての衝突をシミュレート
        mtrl.Diffuse = Color(1.0f, 1.0f, 0.0f, 0.6f);
        mtrl.TextureEnable = true;
        m_textures[4]->ModifyMtrl(mtrl);

        // 静的描画、スケールは 0.5 に設定
        m_textures[4]->Draw(
            Vector3(0.5f, 0.5f, 1.0f),
            Vector3(0, 0, 0),
            Vector3(screenPos.x, screenPos.y, 0)
        );
    }
}