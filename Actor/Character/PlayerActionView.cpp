#include "PlayerActionView.h"

#include "../../System/Renderer.h"
#include "../../System/CStaticMeshRenderer.h"
#include "../../System/MeshManager.h"
#include "../../System/ZFightTunables.h"
#include "../../Types/Direction.h"
#include "../../Core/GameContext.h"
#include "../../GamePlay/Manager/MapManager.h"
#include "../Gimmick/Trap.h"
#include "../Base/Unit.h"

namespace {
    const float GHOST_ALPHA = 0.6f;   // ゴースト（残像）の透明度

    // --- 床ヒントUIの表示色 ---
    const Color MOVE_RANGE_COLOR = Color(0.0f, 1.0f, 0.0f, 0.4f);     // 移動範囲：半透明の緑
    const Color ATTACK_HINT_COLOR = Color(0.9f, 0.0f, 0.0f, 0.2f);    // 攻撃可能方向：薄い赤
    const Color ATTACK_SELECTED_COLOR = Color(0.9f, 0.0f, 0.0f, 0.8f);// 選択中の攻撃方向：濃い赤

    // --- 経路矢印の表示色 ---
    const Color PATH_NORMAL_COLOR = Color(1.0f, 1.0f, 1.0f, 1.0f);    // 白（通常時）
    const Color PATH_DANGER_COLOR = Color(1.0f, 0.0f, 0.0f, 1.0f);    // 赤（目的地に罠がある時）
}

void PlayerActionView::Init(GameContext* context) {
    m_context = context;
    m_pathLineRenderer = MeshManager::GetRenderer<CStaticMeshRenderer>("arrow_straight_mesh");
    m_pathCornerRenderer = MeshManager::GetRenderer<CStaticMeshRenderer>("arrow_corner_mesh");
}

void PlayerActionView::DrawMoveRange(const std::vector<Tile*>& rangeTiles) {
    m_context->GetMapManager()->DrawColoredTiles(rangeTiles, MOVE_RANGE_COLOR);
}

void PlayerActionView::DrawGhost(CStaticMeshRenderer* body, const Vector3& scale, float rotY,
    int startGX, int startGZ, const Matrix4x4& restoreWorld) {
    if (!body) return;

    Vector3 ghostPos = m_context->GetMapManager()->GetWorldPosition(startGX, startGZ);
    ghostPos.y += ZFight::Ghost;

    Matrix4x4 ghostWorld = Matrix4x4::CreateScale(scale)
        * Matrix4x4::CreateRotationY(rotY)
        * Matrix4x4::CreateTranslation(ghostPos);

    Renderer::SetWorldMatrix(&ghostWorld);

    for (int i = 0; CMaterial * mtrl = body->GetMaterial(i); ++i) {
        MATERIAL m = mtrl->GetData();
        m.Diffuse.w = GHOST_ALPHA;
        mtrl->SetMaterial(m);
    }

    body->Draw();

    for (int i = 0; CMaterial * mtrl = body->GetMaterial(i); ++i) {
        MATERIAL m = mtrl->GetData();
        m.Diffuse.w = 1.0f;
        mtrl->SetMaterial(m);
    }

    // Player 本体のワールド行列へ復元（描画前の状態に戻す）
    Matrix4x4 world = restoreWorld;
    Renderer::SetWorldMatrix(&world);
}

void PlayerActionView::DrawPathLine(const std::vector<Tile*>& path, int startGX, int startGZ) {
    // スタート地点は描画しない
    if (path.empty()) return;

    // 目的地が危険（罠）かどうかをチェック
    bool isDanger = (Trap::GetArmedTrap(path.back()) != nullptr);

    Color targetColor = isDanger ? PATH_DANGER_COLOR : PATH_NORMAL_COLOR;

    Tile* startTile = m_context->GetMapManager()->GetTile(startGX, startGZ);
    for (size_t i = 0; i < path.size(); ++i) {
        Tile* currTile = path[i];
        if (currTile->gridX == startGX && currTile->gridZ == startGZ) continue;

        Tile* prevTile = (i == 0) ? startTile : path[i - 1];
        Tile* nextTile = (i + 1 < path.size()) ? path[i + 1] : nullptr;

        Vector3 pos = m_context->GetMapManager()->GetWorldPosition(currTile->gridX, currTile->gridZ);
        pos.y += ZFight::PathLine;

        float rotY = 0.0f;
        CStaticMeshRenderer* rendererToUse = nullptr;

        if (nextTile) {
            // 中間タイル
            int dxIn = currTile->gridX - prevTile->gridX;
            int dzIn = currTile->gridZ - prevTile->gridZ;
            int dxOut = nextTile->gridX - currTile->gridX;
            int dzOut = nextTile->gridZ - currTile->gridZ;

            if (dxIn == dxOut && dzIn == dzOut) {
                // 直線：入りと出の向きが一致
                rendererToUse = m_pathLineRenderer;
                rotY = CalculateLineRotation(dxIn, dzIn);
            }
            else {
                // 角：入りと出の向きが違う
                rendererToUse = m_pathCornerRenderer;
                int n1x = prevTile->gridX - currTile->gridX;
                int n1z = prevTile->gridZ - currTile->gridZ;
                int n2x = nextTile->gridX - currTile->gridX;
                int n2z = nextTile->gridZ - currTile->gridZ;
                rotY = CalculateCornerRotation(n1x, n1z, n2x, n2z);
            }
        }
        else {
            // ゴールタイル
            int dx = currTile->gridX - prevTile->gridX;
            int dz = currTile->gridZ - prevTile->gridZ;

            rendererToUse = m_pathLineRenderer;
            rotY = CalculateLineRotation(dx, dz);
        }

        if (rendererToUse) {
            Matrix4x4 world = Matrix4x4::CreateScale(Vector3(1.0f, 1.0f, 1.0f))
                * Matrix4x4::CreateRotationY(rotY)
                * Matrix4x4::CreateTranslation(pos);

            Renderer::SetWorldMatrix(&world);
            if (auto* mat = rendererToUse->GetMaterial(0)) {
                MATERIAL original = mat->GetData();
                MATERIAL temp = original;
                temp.Diffuse = targetColor;
                mat->SetMaterial(temp);
                rendererToUse->Draw();
                mat->SetMaterial(original);
            }
            else {
                rendererToUse->Draw();
            }
        }
    }
}

void PlayerActionView::DrawAttackWarningFloor(int gridX, int gridZ, Direction attackDir) {
    // 全攻撃できる方向のヒント UI の描画
    std::vector<Tile*> neighbors;
    int dx[] = { 0, 0, -1, 1 };
    int dz[] = { 1, -1, 0, 0 }; // 四近傍
    for (int i = 0; i < 4; ++i) {
        Tile* t = m_context->GetMapManager()->GetTile(gridX + dx[i], gridZ + dz[i]);
        if (t) neighbors.push_back(t);
    }
    m_context->GetMapManager()->DrawColoredTiles(neighbors, ATTACK_HINT_COLOR);
    // 現在選択中のタイル（攻撃方向）を描画
    DirOffset offset = DirOffset::From(attackDir);
    Tile* target = m_context->GetMapManager()->GetTile(gridX + offset.x, gridZ + offset.z);
    if (target) {
        std::vector<Tile*> one{ target };
        m_context->GetMapManager()->DrawColoredTiles(one, ATTACK_SELECTED_COLOR);
    }
}

void PlayerActionView::DrawAttackWarningOverlay(int gridX, int gridZ, Direction attackDir, bool isPush, Unit* self) {
    DirOffset offset = DirOffset::From(attackDir);
    Tile* target = m_context->GetMapManager()->GetTile(gridX + offset.x, gridZ + offset.z);
    // === ノックバック UI プレビューをトリガー ===
    if (target && isPush && target->occupant && target->occupant != self) {
        target->occupant->DrawPushPreview(attackDir);
    }
}

float PlayerActionView::CalculateLineRotation(int dx, int dz) {
    if (dx == 1) return 0.0f;           // 右（デフォルト）
    if (dx == -1) return PI;            // 左 (180度)
    if (dz == 1) return -PI / 2.0f;     // 上 (-90度)
    if (dz == -1) return PI / 2.0f;     // 下 (+90度)
    return 0.0f;
}

float PlayerActionView::CalculateCornerRotation(int dx1, int dz1, int dx2, int dz2) {
    bool left = (dx1 == -1 || dx2 == -1);
    bool right = (dx1 == 1 || dx2 == 1);
    bool up = (dz1 == 1 || dz2 == 1);
    bool down = (dz1 == -1 || dz2 == -1);

    if (left && down) return 0.0f;         // Left + Down -> 0
    if (down && right) return -PI / 2.0f;  // Down + Right -> -90
    if (right && up) return PI;            // Right + Up -> 180
    if (up && left) return PI / 2.0f;      // Up + Left -> 90
    return 0.0f;
}