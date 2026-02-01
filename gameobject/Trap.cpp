#include "Trap.h"
#include "../system/meshmanager.h"
#include "../gameobject/Unit.h"
#include <iostream>

void Trap::Init(MapModelType type, Vector3 position)
{
    // 基底クラスの初期化と、トラップの占有サイズ（通常は1x1）を取得
    MapObject::Init(type, position);
    GetDimensions(type, m_sizeX, m_sizeZ);

    // 1. テクスチャパスの確定
    // assets/texture/furniture/trap.png 等、トラップ専用のリソースを指定
    std::string texPath = "assets/texture/furniture/trap.png";

    // === コンストラクタ呼び出しとロードを分ける ===
    // エラー原因：CTexture(string) というコンストラクタが存在しないため
    m_texture = std::make_unique<CTexture>(); // 引数なしで作成
    m_texture->Load(texPath);                 // Load関数で読み込み
    // ==========================================================


    m_renderer = MeshManager::getRenderer<CStaticMeshRenderer>("prop_plane_mesh");

    if (!m_renderer) {
        OutputDebugStringA("[Prop] Warning: range_panel_mesh not found, using floor_mesh.\n");
        m_renderer = MeshManager::getRenderer<CStaticMeshRenderer>("floor_mesh");
    }


    // 高さの自動計算 (Scale Z)
    float visualHeight = 1.0f;
    if (m_texture && m_texture->GetWidth() > 0) {
        float ratio = (float)m_texture->GetHeight() / (float)m_texture->GetWidth();
        visualHeight = (float)m_sizeX * ratio;
    }

    m_srt.rot.x = 0.0f;

    m_srt.scale = Vector3((float)m_sizeX*0.9f, visualHeight*0.9f, 0.9f);

    m_srt.pos = position;

    m_srt.pos.x += 0.05f;
    m_srt.pos.y += 0.01f;
    m_srt.pos.z -= 0.5f;

    // [アニメーション用] 初期スケールを保存
    m_initialScale = m_srt.scale;

    UpdateWorldMatrix();
}

void Trap::OnEnter(Unit* unit)
{
    // 既に発動済みの場合は処理をスキップ
    if (m_isActivated) return;

    if (unit) {
        // デバッグ出力：トラップ発動
        std::cout << "TRAP ACTIVATED! Unit took damage." << std::endl;

        // ユニットに2ポイントのダメージを与える（攻撃者は無し）
        unit->TakeDamage(6, nullptr);

        // 発動済みフラグを立てる
        m_isActivated = true;
        m_isAnimating = true;   // 【新規】消失アニメーションの再生を開始
        m_animTimer = 0.0f;     // タイマーをリセットして最初から再生
    }
}

void Trap::OnDraw(uint64_t delta) {
    if (m_srt.scale.x <= 0.001f) return;
    if (!m_renderer) return;

    Renderer::SetBlendState(BS_ALPHABLEND);
    Renderer::SetDepthEnable(true);
    Renderer::SetWorldMatrix(&m_WorldMatrix);

	// テクスチャを GPU にセット
    if (m_texture) {
        m_texture->SetGPU();
    }

	// マテリアルの透明度をアニメーションに応じて調整
    if (auto* mat = m_renderer->GetMaterial(0)) {
        MATERIAL m = mat->GetData();
        m.TextureEnable = TRUE;
        m.Ambient = Color(1.0f, 1.0f, 1.0f, 1.0f);
        m.Diffuse = Color(1.0f, 1.0f, 1.0f, 1.0f);
		// アニメーション中は透明度を調整
        if (m_isAnimating) {
            float t = m_animTimer / m_animDuration;
            if (t > 1.0f) t = 1.0f;
            m.Diffuse.w = 1.0f - t;
        }
        else {
            m.Diffuse.w = 1.0f;
        }
        mat->SetMaterial(m);
    }

    m_renderer->Draw();

    Renderer::SetBlendState(BS_NONE);
}

void Trap::GetDimensions(MapModelType type, int& outW, int& outD)
{
    switch (type) {
    case MapModelType::TRAP: default: outW = 1; outD = 1; break;
    }
}

void Trap::Update(uint64_t dt) {
    // アニメーション再生中でない場合は何もしない
    if (!m_isAnimating) return;

    // ミリ秒を秒単位に変換
    float deltaSeconds = (float)dt / 1000.0f;
    m_animTimer += deltaSeconds;

    // アニメーションの進捗率を計算 (0.0 ～ 1.0)
    float t = m_animTimer / m_animDuration;

    if (t >= 1.0f) {
        t = 1.0f;
        m_isAnimating = false;
        // アニメーション終了：完全に非表示にする
        m_srt.scale = Vector3(0, 0, 0);
        // 必要に応じてここで m_isDead = true; を設定し、シーンから削除する
    }
    else {
        // 【演出1】スケール：初期サイズから 0 へと徐々に小さくする
        // 線形補間（Lerp）に近い計算：Current = Start * (1 - t)
        m_srt.scale = m_initialScale * (1.0f - t);

        // 【演出2】透明度（フェードアウト）：1.0 から 0.0 へ
        // マテリアルの Diffuse アルファ値（w成分）を書き換える
        if (m_renderer) {
            CMaterial* mat = m_renderer->GetMaterial(0);
            if (mat) {
                MATERIAL m = mat->GetData();
                m.Diffuse.w = 1.0f - t; // w は Alpha チャンネル
                mat->SetMaterial(m);    // 変更を GPU 定数バッファへ反映
            }
        }
    }

    // 行列を再計算して描画に反映
    UpdateWorldMatrix();
}