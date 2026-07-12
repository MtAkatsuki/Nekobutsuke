#include "Prop.h"
#include "../../System/MeshManager.h"
#include "../../System/ZFightTunables.h"
#include "../../Core/GameContext.h"
#include "../Character/Player.h"      
#include "../../Actor/Character/Ally.h"        
#include "../../GamePlay/Manager/EnemyManager.h"   
#include "../Character/Enemy.h"

namespace {
    // 半透明化（オクルージョン）制御の定数
    const float OCCLUDED_ALPHA = 0.4f;      // ユニットが隠れた時の透明度
    const float NORMAL_ALPHA = 1.0f;        // 通常時の透明度
    const float ALPHA_FADE_SPEED = 5.0f;    // 透明度の遷移スピード
    const int   OCCLUSION_DEPTH = 1;        // オブジェクトの裏何マスまでを「隠れている」と判定するか
}

void Prop::Init(MapModelType type, Vector3 position) {
    // 1. 基本初期化とサイズの取得
    MapObject::Init(type, position); auto size = GetModelSize(type);
    auto [w, d] = GetModelSize(type);
    m_sizeX = w;
    m_sizeZ = d;

    // 2. 物件タイプに応じたメッシュ名の解決（プロップ定義テーブル参照）
    const PropDef* def = FindPropDef(type);
    std::string meshName = (def && def->meshName) ? def->meshName : "prop_plane_mesh";

    m_renderer = MeshManager::GetRenderer<CStaticMeshRenderer>(meshName);
    if (!m_renderer) {
        OutputDebugStringA("[Prop] Warning: Specific 3D mesh not found, using floor_mesh.\n");
        m_renderer = MeshManager::GetRenderer<CStaticMeshRenderer>("floor_mesh");
    }

    m_srt.scale = Vector3(1.0f, 1.0f, 1.0f);
    m_srt.rot = Vector3(0.0f, 0.0f, 0.0f);
    m_srt.pos = position;

    UpdateWorldMatrix();
}

void Prop::Update(float deltaSeconds) {
    if (!m_context) return;

    // 遮蔽（オクルージョン）検知：ユニットが家具のすぐ裏に居るか

    // コンテナへ収集せず、その場で判定して短絡する（毎フレームのヒープ確保を回避）
    int propMinX = m_gridX;
    int propMaxX = m_gridX + m_sizeX - 1;
    int propMaxZ = m_gridZ + m_sizeZ - 1;

    // 距離制限：家具の後ろ深く（遠く）にいる場合は、視覚的に問題ないため透明化を解除する
    int occlusionLimitZ = propMaxZ + OCCLUSION_DEPTH;

    auto isBehind = [&](const Unit* unit) {
        if (!unit) return false;
        int unitX = unit->GetUnitGridX();
        int unitZ = unit->GetUnitGridZ();
        // 家具の幅(X軸)に収まっており、かつ家具のすぐ裏(Z軸)にいる場合
        return unitX >= propMinX && unitX <= propMaxX &&
            unitZ >= m_gridZ && unitZ <= occlusionLimitZ;
        };

    bool isOccluding = isBehind(m_context->GetPlayer()) || isBehind(m_context->GetAlly());
    if (!isOccluding && m_context->GetEnemyManager()) {
        for (const auto* e : m_context->GetEnemyManager()->GetAllEnemies()) {
            if (e && !e->IsDead() && isBehind(e)) { isOccluding = true; break; }
        }
    }

    // 状態に応じた目標アルファの設定と線形補間（Lerp）によるフェード
    m_targetAlpha = isOccluding ? OCCLUDED_ALPHA : NORMAL_ALPHA;
    m_currentAlpha += (m_targetAlpha - m_currentAlpha) * ALPHA_FADE_SPEED * deltaSeconds;

    UpdateWorldMatrix();
}

void Prop::OnDraw(float /*deltaSeconds*/) {
    if (!m_renderer) return;

    DrawPropShadow();

    if (!m_toonShader) m_toonShader = MeshManager::GetShader<CShader>("toonshader");
    if (m_toonShader) m_toonShader->SetGPU();

    // 半透明描画のためのステート設定
    Renderer::SetBlendState(BS_ALPHABLEND);
    Renderer::SetDepthEnable(true);
    Renderer::SetWorldMatrix(&m_worldMatrix);

    m_renderer->Draw();
    Renderer::SetBlendState(BS_NONE);
}

void Prop::DrawPropShadow() {
    if (!Renderer::s_shadowEnabled) return;
    if (!m_blobShader) m_blobShader = MeshManager::GetShader<CShader>("blobshader");
    if (!m_blobMesh)   m_blobMesh = MeshManager::GetRenderer<CStaticMeshRenderer>("range_panel_mesh"); // 1x1 plane
    if (!m_toonShader) m_toonShader = MeshManager::GetShader<CShader>("toonshader");
    if (!m_blobShader || !m_blobMesh || !m_toonShader) return;

    Vector3 p = m_srt.pos;              // Propのposは占有範囲の中心位置（Init内でoffset加算済み）
    p.y = ZFight::Blob;
    float gx = m_sizeX;
    float gz = m_sizeZ;
    Matrix4x4 w = Matrix4x4::CreateScale(gx, 1.0f, gz) * Matrix4x4::CreateTranslation(p);
    Renderer::SetWorldMatrix(&w);

    m_blobShader->SetGPU();
    Renderer::SetBlendState(BS_ALPHABLEND);
    Renderer::DisableCulling(false);   // 片面plane → 両面描画
    Renderer::SetDepthReadOnly();      // キャラクターや他の影とのz-fightingを防止
    m_blobMesh->Draw();

    Renderer::SetDepthEnable(true);
    Renderer::DisableCulling(true);
    Renderer::SetBlendState(BS_NONE);
    MeshManager::GetShader<CShader>("toonshader")->SetGPU(); // toonへ復元
}