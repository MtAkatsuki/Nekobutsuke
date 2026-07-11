#include "Trap.h"
#include "../../System/MeshManager.h"
#include "../../System/ZFightTunables.h"
#include "../Base/Unit.h"
#include "../../Core/DebugLog.h"
#include "../../GamePlay/Manager/MapManager.h"

namespace {
    // 演出・バランス用定数
    const float TRAP_INITIAL_SCALE = 0.5f;  // トラップの基準スケール
    const float ANIM_DURATION = 0.5f;       // 消失アニメーションの再生時間（秒）
}

void Trap::Init(MapModelType type, Vector3 position) {
    MapObject::Init(type, position);
    GetDimensions(type, m_sizeX, m_sizeZ);

    m_renderer = MeshManager::GetRenderer<CStaticMeshRenderer>("trap_mesh");
    if (!m_renderer) {
        OutputDebugStringA("[Trap] Warning: trap_mesh not found.\n");
    }

    m_srt.scale = Vector3(TRAP_INITIAL_SCALE, TRAP_INITIAL_SCALE, TRAP_INITIAL_SCALE);
    m_srt.rot = Vector3(0.0f, 0.0f, 0.0f);
    m_srt.pos = position;
    m_srt.pos.y += ZFight::Trap;

    // アニメーション計算用の基準スケールと透明度を保存
    m_initialScale = m_srt.scale;
    m_currentAlpha = 1.0f;

    UpdateWorldMatrix();
}

void Trap::Update(uint64_t dt) {
    // 消失アニメーション中でなければ計算をスキップ
    if (!m_isDisappearing) return;

    float deltaSeconds = static_cast<float>(dt) / 1000.0f;
    m_animTimer += deltaSeconds;

    // 進行度の正規化 (0.0 ～ 1.0)
    float t = m_animTimer / ANIM_DURATION;

    if (t >= 1.0f) {
        t = 1.0f;
        m_isDisappearing = false;

        // 完全に消失させる
        m_srt.scale = Vector3(0, 0, 0);
        m_currentAlpha = 0.0f;
    }
    else {
        // 【ロジック責務】：Update内では「数学的な計算」のみを行う
        // スケールを徐々に小さくする (1.0 -> 0.0)
        m_srt.scale = m_initialScale * (1.0f - t);

        // 透明度（Alpha）をフェードアウトさせる (1.0 -> 0.0)
        m_currentAlpha = 1.0f - t;
    }

    UpdateWorldMatrix();
}

void Trap::OnDraw(uint64_t delta) {
    // 完全に縮小されている（消失済み）場合は描画をスキップし、GPU負荷を軽減する
    if (m_srt.scale.x <= 0.001f || !m_renderer) return;

    if (!m_toonShader) m_toonShader = MeshManager::GetShader<CShader>("toonshader");
    if (m_toonShader) m_toonShader->SetGPU();

    Renderer::SetBlendState(BS_ALPHABLEND);
    Renderer::SetDepthEnable(true);
    Renderer::SetWorldMatrix(&m_worldMatrix);

    // 【描画責務】：Updateで計算された m_currentAlpha をマテリアルに適用するだけ
    if (auto* mat = m_renderer->GetMaterial(0)) {
        MATERIAL m = mat->GetData();
        m.TextureEnable = TRUE;
        m.Ambient = Color(1.0f, 1.0f, 1.0f, 1.0f);
        m.Diffuse = Color(1.0f, 1.0f, 1.0f, m_currentAlpha);
        mat->SetMaterial(m);
    }

    m_renderer->Draw();

    Renderer::SetBlendState(BS_NONE);
}

void Trap::OnEnter(Unit* unit) {
    // 重複発動の防止
    if (m_hasActivated) return;

    if (unit) {
        DBG_ERROR("[Trap] TRAP ACTIVATED! Unit took damage.");
        // ユニットにトラップダメージを適用（攻撃者=nullptr）
        unit->TakeDamage(m_trapDamage, nullptr);

        // 発動状態を確定し、消失演出へ移行
        m_hasActivated = true;
        m_isDisappearing = true;
        m_animTimer = 0.0f;
    }
}


void Trap::GetDimensions(MapModelType type, int& outW, int& outD)
{
    const PropDef* def = FindPropDef(type);
    outW = def ? def->sizeX : 1;
    outD = def ? def->sizeZ : 1;
}

Trap* Trap::GetArmedTrap(const Tile* tile) {
    if (!tile || !tile->structure) return nullptr;
    if (tile->structure->GetType() != MapModelType::TRAP) return nullptr;

    Trap* trap = dynamic_cast<Trap*>(tile->structure);
    return (trap && !trap->IsActivated()) ? trap : nullptr;
}