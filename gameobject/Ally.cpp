#include "Ally.h"
#include "../manager/GameContext.h"
#include "../manager/MapManager.h"
#include "../system/meshmanager.h"
#include "../utility/WorldToScreen.h"
#include "../Application.h"
#include "../ui/DialogueUI.h"
#include "../manager/AudioManager.h"
#include "../manager/EffectManager.h"

void Ally::Init() 
{   //モデル関連のソースをロード　
        // ---  Model Front ---
    {
        std::unique_ptr<CStaticMesh> mesh = std::make_unique<CStaticMesh>();
        mesh->Load("assets/model/character/ally_front.obj", "assets/model/character/");
        std::unique_ptr<CStaticMeshRenderer> renderer = std::make_unique<CStaticMeshRenderer>();
        renderer->Init(*mesh);
        MeshManager::RegisterMesh<CStaticMesh>("ally_front_mesh", std::move(mesh));
        MeshManager::RegisterMeshRenderer<CStaticMeshRenderer>("ally_front_mesh", std::move(renderer));
    }
    // ---  Model Back ---
    {
        std::unique_ptr<CStaticMesh> mesh = std::make_unique<CStaticMesh>();
        mesh->Load("assets/model/character/ally_back.obj", "assets/model/character/");
        std::unique_ptr<CStaticMeshRenderer> renderer = std::make_unique<CStaticMeshRenderer>();
        renderer->Init(*mesh);
        MeshManager::RegisterMesh<CStaticMesh>("ally_back_mesh", std::move(mesh));
        MeshManager::RegisterMeshRenderer<CStaticMeshRenderer>("ally_back_mesh", std::move(renderer));
    }


    m_shader = MeshManager::getShader<CShader>("unlightshader");

    auto* frontR = MeshManager::getRenderer<CStaticMeshRenderer>("ally_front_mesh");
    auto* backR = MeshManager::getRenderer<CStaticMeshRenderer>("ally_back_mesh");
    SetModelRenderers(frontR, backR);


    //初期ステータスを設置
    m_maxHP = 4;
    m_currentHP = m_maxHP;
    m_team = Team::Ally;
    m_maxMovePoints = 0;//現在仲間は移動できない
    m_currentMovePoints = m_maxMovePoints;

    m_srt.scale = Vector3(-1.0f, 1.0f, 1.0f);
    SetFacing(Direction::South);
    UpdateWorldMatrix();
}

void Ally::TakeDamage(int damage, Unit* attacker) {
    Unit::TakeDamage(damage, attacker);
}

void Ally::Update(uint64_t deltatime)
{
    float dt = static_cast<float>(deltatime) / 1000.0f;
    UpdateFlipAnimation(dt);
    if (m_isDigging) {
        UpdateDiggingAnimation(dt);
    }
    UpdateWorldMatrix();
}

void Ally::OnDraw(uint64_t deltatime)
{   //Allyの描画関数
    if (m_shader) m_shader->SetGPU();

  
        Renderer::SetBlendState(BS_ALPHABLEND);
        Renderer::SetDepthEnable(true);

        Renderer::SetWorldMatrix(&m_WorldMatrix);
        DrawModel();

        Renderer::SetBlendState(BS_NONE);
 
}

void Ally::StartTurn() {
	// ターン開始時、採掘アニメーション実行する
    std::cout << "[Ally] Start Turn: Start Digging!" << std::endl;
    m_isDigging = true;
    m_digTimer = 0.0f;
    m_digCount = 0;
    m_hasTriggeredEffect = false;
}

void Ally::UpdateDiggingAnimation(float dt) {
    m_digTimer += dt;

    // 正弦波による揺れのシミュレーション: 0 -> 正 -> 0 -> 負 -> 0
    // 特定の極値点（最大値または最小値）を「地面を叩いた瞬間」として扱う
    // 仕様：負の角度で振り上げ、正の角度で振り下ろす
    float angle = sinf(m_digTimer * DIG_SPEED) * 0.5f; // +/- 0.5ラジアン (約30度)

    // Z軸（左右）またはX軸（前後）の回転。モデルの向きに合わせて調整。
    m_srt.rot.z = angle;

    // 「ヒット（叩いた瞬間）」の判定：
    // 簡易ロジック：角度が 0.4 を超えた（最大傾斜に近い）かつ本周期で未処理の場合
    if (angle > 0.4f && !m_hasTriggeredEffect) {
        // 効果音（SE）の再生
        AudioManager::GetInstance().PlaySE("Dig", 0.6f);

        // エフェクト（足元）の発生
        if (m_context->GetEffectManager()) {
            Vector3 footPos = m_srt.pos;
            // ツルハシの着弾点に合わせてオフセットを調整
            footPos.x += 0.5f;
            m_context->GetEffectManager()->SpawnRubble(footPos, 3);
        }

        m_hasTriggeredEffect = true; // 本周期のフラグを立てる
        m_digCount++; // 採掘カウントをインクリメント
    }

    // トリガーフラグのリセット（反対側に振り戻した際にリセット）
    if (angle < 0.0f) {
        m_hasTriggeredEffect = false;
    }

    // 終了判定
    if (m_digCount >= MAX_DIG_COUNT && angle < 0.1f && angle > -0.1f) {
        // 3回掘り終わり、かつモデルがほぼ中央に戻ったタイミングで停止
        m_isDigging = false;
        m_srt.rot.z = 0.0f; // 姿勢のリセット
        std::cout << "[Ally] Digging Finished." << std::endl;

        // 必要に応じて、ここでUI表示やリザルトへのコールバックを通知
    }

    UpdateWorldMatrix();
}