#include "Prop.h"
#include "../../System/MeshManager.h"
#include "../../System/ZFightTunables.h"

void Prop::Init(MapModelType type, Vector3 position) {
    // 1. 基本初期化とサイズの取得
    MapObject::Init(type, position); 
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

}

void Prop::OnDraw(float deltaSeconds) {
    // 遮蔽フェード：先に fade を更新（完全に非表示でも更新を続け、遮蔽解除時に復帰できるようにする）
    UpdateFade(deltaSeconds);

    if (!m_renderer) return;
    if (m_fade >= 0.999f) return;           // 完全に非表示：影と本体の描画をスキップ

    DrawPropShadow();                        // 部分的なフェード中は接地影を維持（>= 0.999 で全体を非表示）

    if (!m_toonShader) m_toonShader = MeshManager::GetShader<CShader>("toonshader");
    if (m_toonShader) m_toonShader->SetGPU();

    Renderer::SetDepthEnable(true);
    Renderer::SetWorldMatrix(&m_worldMatrix);

    // fade > 0 の場合、マテリアルの Dummy.x に設定 → ToonPS でディザリングクリップ。
    // 描画後に元の値へ戻す
    const bool fading = (m_fade > 0.001f);
    if (fading) ApplyFadeToMaterials(m_fade);
    m_renderer->Draw();
    if (fading) ApplyFadeToMaterials(0.0f);
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