#include "Ally.h"
#include "../manager/GameContext.h"
#include "../manager/MapManager.h"
#include "../system/meshmanager.h"
#include "../utility/WorldToScreen.h"
#include "../Application.h"
#include "../ui/DialogueUI.h"

void Ally::Init() 
{   //モデル関連のソースをロード　
    std::unique_ptr<CStaticMesh> smesh = std::make_unique<CStaticMesh>();
    smesh->Load("assets/model/character/ally.obj", "assets/model/character/");

    std::unique_ptr<CStaticMeshRenderer> srenderer = std::make_unique<CStaticMeshRenderer>();
    srenderer->Init(*smesh);

    MeshManager::RegisterMesh<CStaticMesh>("ally_mesh", std::move(smesh));
    MeshManager::RegisterMeshRenderer<CStaticMeshRenderer>("ally_mesh", std::move(srenderer));

    m_mesh = MeshManager::getMesh<CStaticMesh>("ally_mesh");
    m_renderer = MeshManager::getRenderer<CStaticMeshRenderer>("ally_mesh");
    m_shader = MeshManager::getShader<CShader>("unlightshader");


    //初期ステータスを設置
    m_maxHP = 5;
    m_currentHP = m_maxHP;
    m_team = Team::Ally;
    m_maxMovePoints = 0;//現在仲間は移動できない
    m_currentMovePoints = m_maxMovePoints;

    m_srt.scale = Vector3(-1.0f, 1.0f, 1.0f);
    UpdateWorldMatrix();
}

void Ally::TakeDamage(int damage, Unit* attacker) {
    Unit::TakeDamage(damage, attacker);
}

void Ally::Update(uint64_t deltatime)
{
    UpdateWorldMatrix();
}

void Ally::OnDraw(uint64_t deltatime)
{   //Allyの描画関数
    if (m_shader) m_shader->SetGPU();

    if (m_renderer) {
        Renderer::SetBlendState(BS_ALPHABLEND);
        Renderer::SetDepthEnable(true);

        Renderer::SetWorldMatrix(&m_WorldMatrix);
        m_renderer->Draw();

        Renderer::SetBlendState(BS_NONE);
    }
}