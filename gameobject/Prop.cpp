#include "Prop.h"
#include "../system/meshmanager.h"
#include "../manager/GameContext.h"
#include "../gameobject/player.h"      
#include "../gameobject/Ally.h"        
#include "../manager/EnemyManager.h"   
#include "../gameobject/enemy.h"

namespace {
    // 半透明化（オクルージョン）制御の定数
    const float OCCLUDED_ALPHA = 0.4f;      // ユニットが隠れた時の透明度
    const float NORMAL_ALPHA = 1.0f;        // 通常時の透明度
    const float ALPHA_FADE_SPEED = 5.0f;    // 透明度の遷移スピード
    const int   OCCLUSION_DEPTH = 1;        // オブジェクトの裏何マスまでを「隠れている」と判定するか
}

void Prop::Init(MapModelType type, Vector3 position) {
    // 1. 基本初期化とサイズの取得
    MapObject::Init(type, position);
    GetDimensions(type, m_sizeX, m_sizeZ);

    // 2. 物件タイプに応じたメッシュ名の解決
    std::string meshName = "prop_plane_mesh";
    switch (type) {
    case MapModelType::PROP_SOFA_YOKO: meshName = "sofa_yoko_mesh"; break;
    case MapModelType::PROP_SOFA_TATE: meshName = "sofa_tate_mesh"; break;
    case MapModelType::PROP_TABLE:     meshName = "table_mesh";     break;
    case MapModelType::PROP_BOOKSHELF: meshName = "bookshelf_mesh"; break;
    case MapModelType::PROP_CATTOWER:  meshName = "cattower_mesh";  break;
    default: break;
    }

    m_renderer = MeshManager::getRenderer<CStaticMeshRenderer>(meshName);
    if (!m_renderer) {
        OutputDebugStringA("[Prop] Warning: Specific 3D mesh not found, using floor_mesh.\n");
        m_renderer = MeshManager::getRenderer<CStaticMeshRenderer>("floor_mesh");
    }

    m_srt.scale = Vector3(1.0f, 1.0f, 1.0f);
    m_srt.rot = Vector3(0.0f, 0.0f, 0.0f);
    m_srt.pos = position;

    UpdateWorldMatrix();
}

void Prop::Update(uint64_t delta) {
    if (!m_context) return;
    float dt = static_cast<float>(delta) / 1000.0f;

    // 1. 全ユニットの収集：視認性を確保すべき全対象をリストアップ
    std::vector<Unit*> unitsToCheck;
    if (m_context->GetPlayer()) unitsToCheck.push_back(m_context->GetPlayer());
    if (m_context->GetAlly()) unitsToCheck.push_back(m_context->GetAlly());

    if (m_context->GetEnemyManager()) {
        const auto& enemies = m_context->GetEnemyManager()->GetAllEnemies();
        for (auto* e : enemies) {
            if (e && !e->IsDead()) unitsToCheck.push_back(e);
        }
    }

    // 2. 遮蔽（オクルージョン）検知ロジック
    bool isOccluding = false;
    int propMinX = m_gridX;
    int propMaxX = m_gridX + m_sizeX - 1;
    int propMaxZ = m_gridZ + m_sizeZ - 1;

    // 距離制限：家具の後ろ深く（遠く）にいる場合は、視覚的に問題ないため透明化を解除する
    int occlusionLimitZ = propMaxZ + OCCLUSION_DEPTH;

    for (const auto* unit : unitsToCheck) {
        if (!unit) continue;

        int unitX = unit->GetUnitGridX();
        int unitZ = unit->GetUnitGridZ();

        // 家具の幅(X軸)に収まっており、かつ家具のすぐ裏(Z軸)にいる場合
        if (unitX >= propMinX && unitX <= propMaxX &&
            unitZ >= m_gridZ && unitZ <= occlusionLimitZ) {
            isOccluding = true;
            break;
        }
    }

    // 3. 状態に応じた目標アルファの設定と線形補間（Lerp）によるフェード
    m_targetAlpha = isOccluding ? OCCLUDED_ALPHA : NORMAL_ALPHA;
    m_currentAlpha += (m_targetAlpha - m_currentAlpha) * ALPHA_FADE_SPEED * dt;

    UpdateWorldMatrix();
}

void Prop::OnDraw(uint64_t delta) {
    if (!m_renderer) return;

    auto shader = MeshManager::getShader<CShader>("unlightshader");
    if (shader) {
        shader->SetGPU();
    }

    // 半透明描画のためのステート設定
    Renderer::SetBlendState(BS_ALPHABLEND);
    Renderer::SetDepthEnable(true);
    Renderer::SetWorldMatrix(&m_WorldMatrix);

    // CPU側で計算された m_currentAlpha をマテリアルへ適用（GPUへ転送）
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

void Prop::GetDimensions(MapModelType type, int& outW, int& outD) {
    switch (type) {
    case MapModelType::PROP_SOFA_YOKO:    outW = 3; outD = 1; break;
    case MapModelType::PROP_SOFA_TATE:    outW = 1; outD = 3; break;
    case MapModelType::PROP_TABLE:        outW = 2; outD = 2; break;
    case MapModelType::PROP_BOOKSHELF:    outW = 2; outD = 1; break;
    case MapModelType::PROP_CATTOWER:     outW = 2; outD = 1; break;
    default:                              outW = 1; outD = 1; break;
    }
}