#include "EffectManager.h"
#include "../../Core/GameContext.h"
#include "../../System/Camera.h"
#include "../../System/Utility/WorldToScreen.h"
#include "../../Core/Application.h"
#include "../../System/RandomEngine.h"
#include "../../System/ModelRegistry.h"
#include "../../System/MeshManager.h"
#include "../../System/CStaticMeshRenderer.h"
#include "../../System/FxTunables.h"

namespace {
    const float FX_PI = 3.14159265f;

    // --- 攻撃プレビュー警告アイコン（唯一の 2D 遺産） ---
    const float HIT_PREVIEW_TEX_W = 120.0f;
    const float HIT_PREVIEW_TEX_H = 120.0f;
    const float HIT_PREVIEW_CULL_MARGIN = 100.0f;
    const Color HIT_PREVIEW_COLOR = Color(1.0f, 1.0f, 0.0f, 0.7f); // 半透明の黄色（警告）
    const float HIT_PREVIEW_SCALE = 0.7f;

}

void EffectManager::Init(GameContext* context) {
    m_context = context;

    // 攻撃プレビュー用の警告アイコンのみプリロード（パーティクルはメッシュ描画のため不要）
    m_hitPreviewSprite = std::make_unique<CSprite>(
        HIT_PREVIEW_TEX_W, HIT_PREVIEW_TEX_H, "Assets/texture/effect/hit_effect.png");
}

void EffectManager::Update(float dt) {
    for (auto& p : m_particles3d) {
        if (!p.active) continue;
        p.life -= dt;
        if (p.life <= 0.0f) { p.active = false; continue; }

        // 共通物理：重力 → 速度 → 位置・回転
        p.velocity.y -= p.gravity * dt;
        p.pos += p.velocity * dt;
        p.rotation += p.rotSpeed * dt;

        // 地面バウンド（土塊のみ）：反発しつつ水平減衰
        if (p.bounce && p.pos.y < p.groundY && p.velocity.y < 0.0f) {
            p.pos.y = p.groundY;
            p.velocity.y *= -Fx::Rubble.restitution;
            p.velocity.x *= Fx::Rubble.friction;
            p.velocity.z *= Fx::Rubble.friction;
        }
    }

    // 寿命を迎えたパーティクルの解放 (remove_if イディオム)
    m_particles3d.erase(std::remove_if(m_particles3d.begin(), m_particles3d.end(),
        [](const Particle3D& p) { return !p.active; }), m_particles3d.end());
}

void EffectManager::Clear() {
    m_particles3d.clear();
}

// =========================================================
// 3D パーティクル生成
// 各エフェクトの個性は「運動モデル×形状×色×寿命」の組で表現する：
//   土塊   = 放物線＋バウンド ／ 扁平キューブ ／ 土色     ／ 中寿命
//   打撃   = 直線放射         ／ 細長スパーク ／ 白黄     ／ 超短命
//   バースト= 全方位爆散＋スピン／ 小キューブ  ／ 多色     ／ 中寿命
//   トレイル= 無重力漂い       ／ 小キューブ  ／ 白       ／ 短命
//   スター = 物理なし         ／ 十字ビルボード／ 黄      ／ sin拡縮
// =========================================================

void EffectManager::Spawn3DRubble(const Vector3& worldPos, int count) {
    const auto& P = Fx::Rubble;
    if (count < 0) count = P.count;
    auto& rng = RandomEngine::tls();

    for (int i = 0; i < count; ++i) {
        Particle3D p;
        p.active = true;
        // 足元から少し浮かせて生成（地面へのめり込み防止）
        p.pos = worldPos + Vector3(0.0f, P.spawnLift, 0.0f);

        // 水平はランダム方位に拡散、垂直は必ず上向き → 「掘り出されて跳ねる」弧を描く
        float ang = (float)rng.uniformReal(0.0, 2.0 * FX_PI);
        float side = (float)rng.uniformReal(P.sideMin, P.sideMax);
        p.velocity = Vector3(cosf(ang) * side,
            (float)rng.uniformReal(P.upMin, P.upMax), sinf(ang) * side);

        // 全軸ランダム自転（転がる土塊感）
        p.rotSpeed = Vector3(
            (float)rng.uniformReal(-P.spinMax, P.spinMax),
            (float)rng.uniformReal(-P.spinMax, P.spinMax),
            (float)rng.uniformReal(-P.spinMax, P.spinMax));

        // Y を潰した不揃いキューブ = 土塊のシルエット
        float s = (float)rng.uniformReal(P.scaleMin, P.scaleMax);
        p.baseScale = Vector3(s, s * P.flatten, s);

        p.color = P.colors[rng.uniformInt(0, (int)std::size(P.colors) - 1)];
        p.gravity = P.gravity;
        p.life = p.maxLife = P.life;

        // このエフェクトだけ地面バウンドが有効（カートゥーンの跳ね感）
        p.bounce = true;
        p.groundY = worldPos.y + P.groundLift;

        m_particles3d.push_back(p);
    }
}

void EffectManager::Spawn3DHit(const Vector3& worldPos) {
    const auto& P = Fx::Hit;
    auto& rng = RandomEngine::tls();

    for (int i = 0; i < P.count; ++i) {
        Particle3D p;
        p.active = true;
        p.pos = worldPos;

        // 全方位放射。elev（垂直比率）を水平寄りに偏らせて「パンッ」と弾ける印象にする
        float ang = (float)rng.uniformReal(0.0, 2.0 * FX_PI);
        float elev = (float)rng.uniformReal(P.elevMin, P.elevMax);
        float spd = (float)rng.uniformReal(P.speedMin, P.speedMax);
        p.velocity = Vector3(cosf(ang) * spd, elev * spd, sinf(ang) * spd);

        // 三角カケラを速度方向へ向けて飛ばす（描画時に基底を構築するため回転は不要）
        p.useShard = true;
        p.alignToVelocity = true;
        p.baseScale = P.sparkScale;

        p.color = P.colors[rng.uniformInt(0, (int)std::size(P.colors) - 1)];
        p.gravity = P.gravity;
        p.life = p.maxLife = P.life;
        m_particles3d.push_back(p);
    }
}

void EffectManager::Spawn3DDeathBurst(const Vector3& worldPos) {
    const auto& P = Fx::Burst;
    auto& rng = RandomEngine::tls();

    for (int i = 0; i < P.count; ++i) {
        Particle3D p;
        p.active = true;
        p.pos = worldPos;

        // 全方位＋上向きバイアス：噴水状に打ち上がってから降り注ぐ
        float ang = (float)rng.uniformReal(0.0, 2.0 * FX_PI);
        float spd = (float)rng.uniformReal(P.speedMin, P.speedMax);
        float up = (float)rng.uniformReal(0.0, P.upBias);
        p.velocity = Vector3(cosf(ang) * spd, up + spd * P.upFactor, sinf(ang) * spd);

        // 高速スピン＝紙吹雪のきらめき（toon の明暗バンドがチラつく）
        p.rotSpeed = Vector3(
            (float)rng.uniformReal(-P.spinMax, P.spinMax),
            (float)rng.uniformReal(-P.spinMax, P.spinMax),
            (float)rng.uniformReal(-P.spinMax, P.spinMax));

        float s = (float)rng.uniformReal(P.scaleMin, P.scaleMax);
        p.baseScale = Vector3(s, s, s);

        // 5色からランダム＝華やかさの核
        p.color = P.colors[rng.uniformInt(0, (int)std::size(P.colors) - 1)];
        p.gravity = P.gravity;
        p.life = p.maxLife = P.life;
        m_particles3d.push_back(p);
    }
}

void EffectManager::Spawn3DTrailPuff(const Vector3& worldPos) {
    const auto& P = Fx::Trail;
    auto& rng = RandomEngine::tls();

    // 1個ずつ生成（生成間隔は呼び出し側 Unit::UpdateDeathFly が Fx::Trail.interval で制御）
    Particle3D p;
    p.active = true;
    p.pos = worldPos;

    // 無重力＋微小ドリフト：その場に残って漂う「残気」。飛翔体の軌跡として線状に並ぶ
    p.velocity = Vector3(
        (float)rng.uniformReal(-P.drift, P.drift),
        (float)rng.uniformReal(0.0, P.drift),
        (float)rng.uniformReal(-P.drift, P.drift));
    p.rotSpeed = Vector3(P.spin, P.spin, P.spin);
    p.baseScale = Vector3(P.scale, P.scale, P.scale);
    p.color = P.color;
    p.gravity = 0.0f;
    p.life = p.maxLife = P.life;
    m_particles3d.push_back(p);
}

void EffectManager::Spawn3DStarCross(const Vector3& worldPos) {
    const auto& P = Fx::Star;

    // 物理・乱数なしの1粒子。描画側で「ビルボード十字＋sin拡縮」の特殊パスを通る
    Particle3D p;
    p.active = true;
    p.pos = worldPos;
    p.color = P.color;
    p.life = p.maxLife = P.life;
    p.isStar = true;
    m_particles3d.push_back(p);
}

// =========================================================
// 描画
// =========================================================
void EffectManager::Draw3D() {
    if (m_particles3d.empty()) return;

    // 遅延取得（GameScene のリソース登録順に依存しないように）
    if (!m_boxRenderer) m_boxRenderer = ModelRegistry::RegisterModel(
        "fx_particle_box", "Assets/model/obj/fx_cube.obj", "Assets/model/obj");
    if (!m_shardRenderer) m_shardRenderer = ModelRegistry::RegisterModel(
        "fx_particle_shard", "Assets/model/obj/fx_shard.obj", "Assets/model/obj");
    if (!m_fxShader) m_fxShader = MeshManager::GetShader<CShader>("fxshader");
    if (!m_boxRenderer || !m_fxShader || !m_boxRenderer->GetMaterial(0)) return;

    Camera* cam = m_context ? m_context->GetCamera() : nullptr;

    // ビルボード基底（ビュー行列回転部の転置 = カメラ基底）：スター用
    Matrix4x4 bb = Matrix4x4::Identity;
    if (cam) {
        Matrix4x4 v = cam->GetViewMatrix();
        bb._11 = v._11; bb._12 = v._21; bb._13 = v._31;
        bb._21 = v._12; bb._22 = v._22; bb._23 = v._32;
        bb._31 = v._13; bb._32 = v._23; bb._33 = v._33;
    }

    // 全 subset のマテリアルへ色を反映
    auto setAllMaterials = [](CStaticMeshRenderer* r, const Color& c) {
        for (int i = 0; ; ++i) {
            CMaterial* mat = r->GetMaterial(i);
            if (!mat) break;
            MATERIAL m = mat->GetData();
            m.Diffuse = c;
            m.TextureEnable = FALSE;
            mat->SetMaterial(m);
        }
        };

    m_fxShader->SetGPU();   // 無光沢（発光体）：Material.Diffuse を直接出力
    if (Fx::Render.additive) {
        Renderer::SetBlendState(BS_ADDITIVE);
        Renderer::DisableCulling(false);   // 三角カケラは単面のため両面描画
        Renderer::SetDepthReadOnly();   // 加算時は深度書込を止め、順序起因のチラつきを防ぐ
    }
    else {
        Renderer::SetBlendState(BS_ALPHABLEND);
        Renderer::DisableCulling(false);   // 三角カケラは単面のため両面描画
        Renderer::SetDepthEnable(true);
    }


    for (const auto& p : m_particles3d) {
        if (!p.active) continue;
        float lifeRatio = p.life / p.maxLife;

        Color c = p.color;
        c.w = p.color.w * ((lifeRatio < Fx::Curve.fadeStartRatio)
            ? lifeRatio / Fx::Curve.fadeStartRatio : 1.0f);
        setAllMaterials(m_boxRenderer, c);

        CStaticMeshRenderer* mesh =
            (p.useShard && m_shardRenderer) ? m_shardRenderer : m_boxRenderer;
        setAllMaterials(mesh, c);

        if (p.isStar) {
            // 四芒星：三角カケラ4枚を billboard 平面内で 90° ずつ、尖端を外向きに配置
            CStaticMeshRenderer* starMesh = m_shardRenderer ? m_shardRenderer : m_boxRenderer;
            setAllMaterials(starMesh, c);
            float prog = 1.0f - lifeRatio;
            float s = sinf(FX_PI * prog);
            Matrix4x4 trans = Matrix4x4::CreateTranslation(p.pos);
            for (int arm = 0; arm < 4; ++arm) {
                Matrix4x4 w = Matrix4x4::CreateScale(
                    Fx::Star.armLen * s, Fx::Star.armThick * s, 1.0f)
                    * Matrix4x4::CreateRotationZ(arm * FX_PI * 0.5f)
                    * bb * trans;
                Renderer::SetWorldMatrix(&w);
                starMesh->Draw();
            }
        }
        else if (p.alignToVelocity) {
            // 速度方向へ +X を向ける正規直交基底を構築（オイラー角を経由しない）
            Vector3 ax = p.velocity;
            if (ax.LengthSquared() < 0.0001f) ax = Vector3(1, 0, 0);
            ax.Normalize();
            Vector3 up = (fabsf(ax.y) > 0.99f) ? Vector3(0, 0, 1) : Vector3(0, 1, 0);
            Vector3 az = ax.Cross(up); az.Normalize();
            Vector3 ay = az.Cross(ax);

            Matrix4x4 rot = Matrix4x4::Identity;
            rot._11 = ax.x; rot._12 = ax.y; rot._13 = ax.z;
            rot._21 = ay.x; rot._22 = ay.y; rot._23 = ay.z;
            rot._31 = az.x; rot._32 = az.y; rot._33 = az.z;

            float shrink = (lifeRatio < Fx::Curve.shrinkStartRatio)
                ? lifeRatio / Fx::Curve.shrinkStartRatio : 1.0f;
            Matrix4x4 scale = Matrix4x4::CreateScale(p.baseScale * shrink);
            Matrix4x4 trans = Matrix4x4::CreateTranslation(p.pos);

            // 単面カケラは真横から見ると線になるため、長軸回りに 90° 回した2枚目で十字刃にする
            Matrix4x4 w1 = scale * rot * trans;
            Renderer::SetWorldMatrix(&w1);
            mesh->Draw();
            Matrix4x4 w2 = scale * Matrix4x4::CreateRotationX(FX_PI * 0.5f) * rot * trans;
            Renderer::SetWorldMatrix(&w2);
            mesh->Draw();
        }
        else {
            float shrink = (lifeRatio < Fx::Curve.shrinkStartRatio)
                ? lifeRatio / Fx::Curve.shrinkStartRatio : 1.0f;
            Matrix4x4 w = Matrix4x4::CreateScale(p.baseScale * shrink)
                * Matrix4x4::CreateRotationX(p.rotation.x)
                * Matrix4x4::CreateRotationY(p.rotation.y)
                * Matrix4x4::CreateRotationZ(p.rotation.z)
                * Matrix4x4::CreateTranslation(p.pos);
            Renderer::SetWorldMatrix(&w);
            mesh->Draw();
        }
    }

    Renderer::DisableCulling(true);   // CULL_BACK へ還元
    Renderer::SetBlendState(BS_NONE);
    Renderer::SetDepthEnable(true);
}

void EffectManager::DrawStaticHitPreview(const Vector3& worldPos) {
    if (!m_context || !m_context->GetCamera() || !m_hitPreviewSprite) return;

    Camera* cam = m_context->GetCamera();
    float sw = (float)Application::GetWidth();
    float sh = (float)Application::GetHeight();

    Vector2 screenPos = WorldToScreen(worldPos, cam->GetViewMatrix(), cam->GetProjMatrix(), sw, sh);

    // 画面外のカリング（描画除外）処理
    if (screenPos.x < -HIT_PREVIEW_CULL_MARGIN || screenPos.x > sw + HIT_PREVIEW_CULL_MARGIN ||
        screenPos.y < -HIT_PREVIEW_CULL_MARGIN || screenPos.y > sh + HIT_PREVIEW_CULL_MARGIN) return;

    MATERIAL mtrl;
    // 半透明の黄色に設定し、警告としての衝突をシミュレート
    mtrl.Diffuse = HIT_PREVIEW_COLOR;
    mtrl.TextureEnable = true;
    m_hitPreviewSprite->ModifyMtrl(mtrl);

    m_hitPreviewSprite->Draw(
        Vector3(HIT_PREVIEW_SCALE, HIT_PREVIEW_SCALE, 1.0f),
        Vector3(0, 0, 0),
        Vector3(screenPos.x, screenPos.y, 0));
}