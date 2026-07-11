#include "EffectManager.h"
#include "../../Core/GameContext.h"
#include "../../System/Camera.h"
#include "../../System/Utility/WorldToScreen.h"
#include "../../Core/Application.h"
#include "../../System/RandomEngine.h"

namespace {
    // --- パーティクル・物理シミュレーション用の調整パラメータ ---
    const float GRAVITY = 1200.0f;              // 瓦礫の落下速度（重力加速度）
    const float RUBBLE_MIN_VEL_X = -150.0f;     // 横方向の拡散速度（最小）
    const float RUBBLE_MAX_VEL_X = 150.0f;      // 横方向の拡散速度（最大）
    const float RUBBLE_MIN_VEL_Y = -400.0f;     // 上方向への跳ね返り速度（最小）
    const float RUBBLE_MAX_VEL_Y = -200.0f;     // 上方向への跳ね返り速度（最大）

    const float HIT_EFFECT_DURATION = 0.25f;    // ヒットエフェクトの再生時間
    const float HIT_EFFECT_INITIAL_SCALE = 0.5f;// ヒットエフェクトの初期スケール
    const int   HIT_EFFECT_TEXTURE_INDEX = 4;   // ヒットエフェクトとして登録されたテクスチャのインデックス
    const int   RUBBLE_TEXTURE_COUNT = 4;       // 瓦礫テクスチャの種類数（インデックス 0〜3）

    // --- 瓦礫パーティクルの生成パラメータ ---
    const float RUBBLE_LIFETIME = 1.0f;         // 瓦礫の寿命（秒）
    const float RUBBLE_MIN_SCALE = 0.5f;        // 瓦礫のランダムスケール下限
    const float RUBBLE_MAX_SCALE = 1.2f;        // 瓦礫のランダムスケール上限
    const float RUBBLE_SPIN_FACTOR = 0.05f;     // 横速度から自転速度への変換係数

    // --- 画面外カリングの余白（px） ---
    const float RUBBLE_CULL_MARGIN = 50.0f;
    const float HIT_CULL_MARGIN = 100.0f;

    // --- 衝突プレビュー表示 ---
    const Color HIT_PREVIEW_COLOR = Color(1.0f, 1.0f, 0.0f, 0.7f); // 半透明の黄色（警告）
    const float HIT_PREVIEW_SCALE = 0.7f;
}

void EffectManager::Init(GameContext* context) {
    m_context = context;

    // テクスチャ定義テーブル（幅・高さはテクスチャ実寸px）
    // ※ 並び順はインデックスに対応：0〜3 = 瓦礫、4 = ヒットエフェクト（HIT_EFFECT_TEXTURE_INDEX）
    struct TexDef { float w, h; const char* path; };
    const TexDef texDefs[] = {
        { 50.0f,  46.0f, "Assets/texture/effect/rubble_01.png" },
        { 48.0f,  46.0f, "Assets/texture/effect/rubble_02.png" },
        { 51.0f,  41.0f, "Assets/texture/effect/rubble_03.png" },
        { 42.0f,  50.0f, "Assets/texture/effect/rubble_04.png" },
        { 120.0f, 120.0f, "Assets/texture/effect/hit_effect.png" },
    };
    for (const auto& t : texDefs) {
        m_textures.push_back(std::make_unique<CSprite>(t.w, t.h, t.path));
    }
}

void EffectManager::Update(float dt) {
    for (auto& p : m_particles) {
        if (!p.active) continue;

        p.lifeTime -= dt;
        if (p.lifeTime <= 0.0f) {
            p.active = false;
            continue;
        }

        // パーティクルタイプ別の更新ロジック
        if (p.type == ParticleType::RUBBLE) {
            // 【物理演算レイヤー】：瓦礫・破片などの落下運動
            p.velocity.y += GRAVITY * dt;
            p.pos.x += p.velocity.x * dt;
            p.pos.y += p.velocity.y * dt;
            p.rotation += p.rotSpeed * dt;
        }
        else if (p.type == ParticleType::HIT_EFFECT) {
            // 【アニメーションレイヤー】：ヒット時のスケール曲線（イージング）
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

    // 寿命を迎えたパーティクルのメモリ解放 (remove_if イディオム)
    m_particles.erase(std::remove_if(m_particles.begin(), m_particles.end(),
        [](const EffectParticle& p) { return !p.active; }), m_particles.end());
}

void EffectManager::Clear() {
    m_particles.clear();
}

void EffectManager::SpawnRubble(const Vector3& worldPos, int count) {
    if (!m_context || !m_context->GetCamera()) return;

    // 1. 3Dの足元座標をスクリーン座標に変換
    float sw = (float)Application::GetWidth();
    float sh = (float)Application::GetHeight();
    Vector2 screenPos = WorldToScreen(worldPos, m_context->GetCamera()->GetViewMatrix(), m_context->GetCamera()->GetProjMatrix(), sw, sh);

    // 画面外カリング (境界チェック)
    if (screenPos.x < -RUBBLE_CULL_MARGIN || screenPos.x > sw + RUBBLE_CULL_MARGIN ||
        screenPos.y < -RUBBLE_CULL_MARGIN || screenPos.y > sh + RUBBLE_CULL_MARGIN) return;

    // 2. パーティクルの生成
    auto& rng = RandomEngine::tls();

    for (int i = 0; i < count; ++i) {
        EffectParticle p; 
        p.active = true;
        p.type = ParticleType::RUBBLE;
        p.pos = screenPos;
        p.velocity = Vector2(
            static_cast<float>(rng.uniformReal(RUBBLE_MIN_VEL_X, RUBBLE_MAX_VEL_X)),
            static_cast<float>(rng.uniformReal(RUBBLE_MIN_VEL_Y, RUBBLE_MAX_VEL_Y)));
        p.rotation = 0.0f;
        p.rotSpeed = static_cast<float>(rng.uniformReal(RUBBLE_MIN_VEL_X, RUBBLE_MAX_VEL_X)) * RUBBLE_SPIN_FACTOR; // ランダムな自転速度
        p.scale = static_cast<float>(rng.uniformReal(RUBBLE_MIN_SCALE, RUBBLE_MAX_SCALE));
        p.lifeTime = RUBBLE_LIFETIME;
        p.maxLifeTime = RUBBLE_LIFETIME;
        p.textureIndex = rng.uniformInt(0, RUBBLE_TEXTURE_COUNT - 1);

        m_particles.push_back(p);
    }
}

void EffectManager::SpawnHitEffect(const Vector3& worldPos) {
    if (!m_context || !m_context->GetCamera()) return;

    // 座標変換（ワールド座標からスクリーン座標へ）
    float sw = (float)Application::GetWidth();
    float sh = (float)Application::GetHeight();
    Vector2 screenPos = WorldToScreen(worldPos, m_context->GetCamera()->GetViewMatrix(), m_context->GetCamera()->GetProjMatrix(), sw, sh);

    // 画面外カリング（画面から大きく外れている場合は生成しない）
    if (screenPos.x < -HIT_CULL_MARGIN || screenPos.x > sw + HIT_CULL_MARGIN ||
        screenPos.y < -HIT_CULL_MARGIN || screenPos.y > sh + HIT_CULL_MARGIN) return;

    EffectParticle p;
    p.active = true;
    p.type = ParticleType::HIT_EFFECT; // ヒットエフェクトとして設定
    p.pos = screenPos;
    p.velocity = Vector2(0, 0); // 移動なし
    p.rotation = static_cast<float>(RandomEngine::tls().uniformInt(0, 359)) * PI / 180.0f;// ランダムな初期角度
    p.rotSpeed = 0.0f;
    p.scale = HIT_EFFECT_INITIAL_SCALE;
    p.maxLifeTime = HIT_EFFECT_DURATION;
    p.lifeTime = p.maxLifeTime;
    p.textureIndex = HIT_EFFECT_TEXTURE_INDEX; // hit_effect.png のインデックスを指定

    m_particles.push_back(p);
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

void EffectManager::DrawStaticHitPreview(const Vector3& worldPos) {
    if (!m_context || !m_context->GetCamera()) return;

    Camera* cam = m_context->GetCamera();
    float sw = (float)Application::GetWidth();
    float sh = (float)Application::GetHeight();

    // スクリーン座標に変換
    Vector2 screenPos = WorldToScreen(worldPos, cam->GetViewMatrix(), cam->GetProjMatrix(), sw, sh);

    // 画面外のカリング（描画除外）処理
    if (screenPos.x < -HIT_CULL_MARGIN || screenPos.x > sw + HIT_CULL_MARGIN ||
        screenPos.y < -HIT_CULL_MARGIN || screenPos.y > sh + HIT_CULL_MARGIN) return;

    // ヒットエフェクト用テクスチャがロード済みか確認
    if (m_textures.size() > HIT_EFFECT_TEXTURE_INDEX && m_textures[HIT_EFFECT_TEXTURE_INDEX]) {

        MATERIAL mtrl;
        // 半透明の黄色に設定し、警告としての衝突をシミュレート
        mtrl.Diffuse = HIT_PREVIEW_COLOR;
        mtrl.TextureEnable = true;
        m_textures[HIT_EFFECT_TEXTURE_INDEX]->ModifyMtrl(mtrl);

        m_textures[HIT_EFFECT_TEXTURE_INDEX]->Draw(
            Vector3(HIT_PREVIEW_SCALE, HIT_PREVIEW_SCALE, 1.0f),
            Vector3(0, 0, 0),
            Vector3(screenPos.x, screenPos.y, 0)
        );
    }
}