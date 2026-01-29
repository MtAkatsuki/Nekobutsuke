#include "Ally.h"
#include "../manager/GameContext.h"
#include "../manager/MapManager.h"
#include "../system/meshmanager.h"
#include "../utility/WorldToScreen.h"
#include "../Application.h"
#include "../ui/DialogueUI.h"

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