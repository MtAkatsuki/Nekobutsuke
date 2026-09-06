#include "PlayerActionView.h"

#include "../../System/Renderer.h"
#include "../../System/CStaticMeshRenderer.h"
#include "../../System/MeshManager.h"
#include "../../System/ZFightTunables.h"
#include "../../Core/GameContext.h"
#include "../Base/Unit.h"
#include <cmath>

void PlayerActionView::Init(GameContext* context) {
    m_context = context;
    m_fxShader = MeshManager::GetShader<CShader>("fxshader");
    m_warnBoxRenderer = MeshManager::GetRenderer<CStaticMeshRenderer>("range_panel_mesh");
}

void PlayerActionView::DrawAimWarningBox(const Vector3& center, float yaw, float size, const Color& color) {
    if (!m_warnBoxRenderer) return;
    if (!m_fxShader) m_fxShader = MeshManager::GetShader<CShader>("fxshader");
    if (!m_fxShader) return;

    Vector3 p = center;
    p.y = ZFight::RangePanel;
    Matrix4x4 world = Matrix4x4::CreateScale(size, 1.0f, size)
        * Matrix4x4::CreateRotationY(yaw)   // 敵方向へ向ける
        * Matrix4x4::CreateTranslation(p);

    m_fxShader->SetGPU();
    Renderer::SetBlendState(BS_ALPHABLEND);
    Renderer::DisableCulling(false);
    Renderer::SetDepthReadOnly();
    Renderer::SetWorldMatrix(&world);

    if (auto* mat = m_warnBoxRenderer->GetMaterial(0)) {
        MATERIAL old = mat->GetData();
        MATERIAL tmp = old;
        tmp.Diffuse = color;
        tmp.TextureEnable = FALSE;   // 純色（Material.Diffuse）
        mat->SetMaterial(tmp);
        m_warnBoxRenderer->Draw();
        mat->SetMaterial(old);
    }

    Renderer::SetDepthEnable(true);
    Renderer::DisableCulling(true);
    Renderer::SetBlendState(BS_NONE);
}
