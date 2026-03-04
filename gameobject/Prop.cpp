#include "Prop.h"
#include "../system/meshmanager.h"
#include "../manager/GameContext.h"
#include "../gameobject/player.h"      
#include "../gameobject/Ally.h"        
#include "../manager/EnemyManager.h"   
#include "../gameobject/enemy.h"

void Prop::Init(MapModelType type, Vector3 position) {
    // 1. 基本的な初期化と占有サイズ（マス数）の取得
    MapObject::Init(type, position);
    GetDimensions(type, m_sizeX, m_sizeZ);

    // 2. 物件タイプに応じた専用3Dメッシュの決定
    std::string meshName = "prop_plane_mesh"; // デフォルト（エラー回避用）
    switch (type) {
    case MapModelType::PROP_SOFA_YOKO: meshName = "sofa_yoko_mesh"; break;
    case MapModelType::PROP_SOFA_TATE: meshName = "sofa_tate_mesh"; break;
    case MapModelType::PROP_TABLE:     meshName = "table_mesh"; break;
    case MapModelType::PROP_BOOKSHELF: meshName = "bookshelf_mesh"; break;
    case MapModelType::PROP_CATTOWER:  meshName = "cattower_mesh"; break;
    default: break;
    }

    // メッシュレンダラーの取得
    m_renderer = MeshManager::getRenderer<CStaticMeshRenderer>(meshName);

    if (!m_renderer) {
        OutputDebugStringA("[Prop] Warning: Specific 3D mesh not found, using floor_mesh.\n");
        m_renderer = MeshManager::getRenderer<CStaticMeshRenderer>("floor_mesh");
    }

    // 3. 3Dモデル用の標準トランスフォーム設定
    // 2D画像用だったアスペクト比計算、X軸の傾き（propAngleX）、Z軸オフセット（-0.6f）はすべて削除
    m_srt.scale = Vector3(1.0f, 1.0f, 1.0f);
    m_srt.rot = Vector3(0.0f, 0.0f, 0.0f);

    // 座標設定 (必要に応じてY軸の微調整を残す)
    m_srt.pos = position;
    // モデルの原点が下端にある前提。もし床にめり込むなら以下の数値を調整してください。
    // m_srt.pos.y += 0.06f; 

    // ワールド行列の更新
    UpdateWorldMatrix();
}

void Prop::OnDraw(uint64_t delta)
{

    if (!m_renderer) return;

    // 専用のunlightshaderを適用
    auto shader = MeshManager::getShader<CShader>("unlightshader");
    if (shader) {
        shader->SetGPU();
    }

    Renderer::SetBlendState(BS_ALPHABLEND);
    Renderer::SetDepthEnable(true);
    Renderer::SetWorldMatrix(&m_WorldMatrix);

    // 半透明（キャラクターが後ろにいる時のフェード）処理
    if (auto* mat = m_renderer->GetMaterial(0)) {
        MATERIAL m = mat->GetData();
        m.TextureEnable = TRUE;
        m.Ambient = Color(1.0f, 1.0f, 1.0f, 1.0f);
        // モデル本来の色(DiffuseのRGB)を維持しつつ、Alpha(w)だけを更新
        m.Diffuse = Color(1.0f, 1.0f, 1.0f, m_currentAlpha);
        mat->SetMaterial(m);
    }

    m_renderer->Draw();
    Renderer::SetBlendState(BS_NONE);
}

// タイルインデックスに基づく遮蔽検知
void Prop::Update(uint64_t delta) {
    if (!m_context) return;
    float dt = (float)delta / 1000.0f;

    // 1. 全ての生存しているユニットを取得
    std::vector<Unit*> unitsToCheck;
    if (m_context->GetPlayer()) unitsToCheck.push_back(m_context->GetPlayer());
    if (m_context->GetAlly()) unitsToCheck.push_back(m_context->GetAlly());

    if (m_context->GetEnemyManager()) {
        const auto& enemies = m_context->GetEnemyManager()->GetAllEnemies();
        for (auto* e : enemies) {
            if (e && !e->IsDead()) unitsToCheck.push_back(e);
        }
    }

    // 2. 遮蔽検知ロジック (グリッドベース)
    bool isOccluding = false;

    // オブジェクトが占有するグリッド範囲
    // X軸範囲: [BaseX, BaseX + SizeX - 1]
    // Z軸範囲: [BaseZ, BaseZ + SizeZ - 1]
    int propMinX = m_gridX;
    int propMaxX = m_gridX + m_sizeX - 1;
    int propMaxZ = m_gridZ + m_sizeZ - 1;

    // 遮蔽判定の最大延長距離
    // オブジェクト自身の範囲、および真後ろ 2マス以内にいるユニットのみ透明化をトリガーする
    // 2マスを超えると、距離が十分離れていると判断し、透明化は不要とする
    int occlusionLimitZ = propMaxZ + 1;

    for (const auto* unit : unitsToCheck) {
        if (!unit) continue;

        int unitX = unit->GetUnitGridX();
        int unitZ = unit->GetUnitGridZ();

        // 判定条件：
        // 1. X軸の重なり
        // 2. Z軸の遮蔽：ユニット Z >= オブジェクト基準 Z
        // 3. [新規追加] Z軸の距離制限：ユニット Z <= 制限距離
        if (unitX >= propMinX && unitX <= propMaxX &&
            unitZ >= m_gridZ && unitZ <= occlusionLimitZ)  // 距離の上限を追加
        {
            isOccluding = true;
            break;
        }
    }

    // 3. 目標透明度の設定
    m_targetAlpha = isOccluding ? 0.4f : 1.0f;

    // 4. 滑らかな遷移（フェード）
    float speed = 5.0f;
    m_currentAlpha += (m_targetAlpha - m_currentAlpha) * speed * dt;

    // 行列の更新
    UpdateWorldMatrix();
}

void Prop::GetDimensions(MapModelType type, int& outW, int& outD) {
    // オブジェクトの種類（タイプ）に応じて、占有するタイルのサイズ（幅と奥行き）を設定
    switch (type) {
    case MapModelType::PROP_SOFA_YOKO:    outW = 3; outD = 1; break; // 横向きのソファ（3x1マス）
    case MapModelType::PROP_SOFA_TATE:    outW = 1; outD = 3; break; // 縦向きのソファ（1x3マス）
    case MapModelType::PROP_TABLE:     outW = 2; outD = 2; break; // テーブル（2x2の正方形エリア）
    case MapModelType::PROP_BOOKSHELF: outW = 2; outD = 1; break; // 本棚（2x1マス）
    case MapModelType::PROP_CATTOWER:  outW = 2; outD = 1; break; // キャットタワー（2x1マス）

        // 壁やその他の単一マスオブジェクト
    default:                           outW = 1; outD = 1; break;
    }
}