#include "Prop.h"
#include "../system/meshmanager.h"
#include "../manager/GameContext.h"

void Prop::Init(MapModelType type, Vector3 position) {
    // 1. 基本的な初期化と占有サイズ（マス数）の取得
    MapObject::Init(type, position);
    GetDimensions(type, m_sizeX, m_sizeZ);

    // 2. 物件タイプに応じたテクスチャパスの決定
    std::string texPath = "assets/texture/furniture/effect000.png";
    switch (type) {
    case MapModelType::PROP_SOFA_YOKO:    texPath = "assets/texture/furniture/sofa_yoko.png"; break; // Enum名にご注意ください(H/V か YOKO/TATE か)
    case MapModelType::PROP_SOFA_TATE:    texPath = "assets/texture/furniture/sofa_tate.png"; break;
    case MapModelType::PROP_TABLE:     texPath = "assets/texture/furniture/table.png"; break;
    case MapModelType::PROP_BOOKSHELF: texPath = "assets/texture/furniture/bookshelf.png"; break;
    case MapModelType::PROP_CATTOWER:  texPath = "assets/texture/furniture/cattower.png"; break;
    case MapModelType::WALL:           texPath = "assets/texture/effect000.png"; break;
    }

    //  テクスチャの読み込み
    // エラー回避のため、インスタンス作成とロードを分ける
    m_texture = std::make_unique<CTexture>();
    m_texture->Load(texPath);

    m_renderer = MeshManager::getRenderer<CStaticMeshRenderer>("prop_plane_mesh");

    if (!m_renderer) {
        OutputDebugStringA("[Prop] Warning: range_panel_mesh not found, using floor_mesh.\n");
        m_renderer = MeshManager::getRenderer<CStaticMeshRenderer>("floor_mesh");
    }


    //  視覚的な高さ（スケールZ）の自動計算
    // 画像のアスペクト比に基づき、占有幅に対して正しい見た目の高さを算出する
    float visualHeight = 1.0f;
    if (m_texture && m_texture->GetWidth() > 0) {
        float ratio = (float)m_texture->GetHeight() / (float)m_texture->GetWidth();
        visualHeight = (float)m_sizeX * ratio;
    }

    m_srt.rot.x = 0.0f;

    // スケール：X軸はグリッド幅に合わせ、Z軸は計算された視覚的な高さに合わせる
    m_srt.scale = Vector3((float)m_sizeX, visualHeight, 1.0f);

    // 座標：Zファイティング防止のためY軸を微調整
    m_srt.pos = position;
    m_srt.pos.y += 0.01f;
    m_srt.pos.z -= 0.6f;

    // ワールド行列の更新
    UpdateWorldMatrix();
}

void Prop::OnDraw(uint64_t delta)
{

    if (!m_renderer) return;
    Renderer::SetBlendState(BS_ALPHABLEND);
    Renderer::SetDepthEnable(true);
    Renderer::SetWorldMatrix(&m_WorldMatrix);

    if (m_texture) {
        m_texture->SetGPU();
    }

    if (auto* mat = m_renderer->GetMaterial(0)) {
        MATERIAL m = mat->GetData();
        m.TextureEnable = TRUE;
        m.Ambient = Color(1.0f, 1.0f, 1.0f, 1.0f);
        m.Diffuse = Color(1.0f, 1.0f, 1.0f, 1.0f);
        mat->SetMaterial(m);  
    }

    m_renderer->Draw();
    Renderer::SetBlendState(BS_NONE);
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