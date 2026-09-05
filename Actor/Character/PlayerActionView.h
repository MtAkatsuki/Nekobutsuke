#pragma once

#include <vector>
#include "../../System/CommonTypes.h"   // Vector3 / Matrix4x4 / Color
#include "../../Types/Direction.h"     // Direction
#include "../../System/CShader.h"

class GameContext;
class CStaticMeshRenderer;
struct Tile;
class Unit;

// =========================================================
// PlayerActionView クラス
// プレイヤーの行動選択中に表示するプレビュー描画をまとめて担当する。
// 移動範囲・経路矢印・出発地点のゴースト・攻撃警告などを扱う。
// =========================================================
class PlayerActionView {
public:
    void Init(GameContext* context);

    // 移動可能マスの表示（緑）
    void DrawMoveRange(const std::vector<Tile*>& rangeTiles);

    // 移動経路の矢印（直線・曲がり）。開始マスは描画せず、終点が罠なら赤で警告表示
    void DrawPathLine(const std::vector<Tile*>& path, int startGX, int startGZ);

    // 出発地点に半透明のゴーストを描画。
    // body は Player 本体のRenderer、restoreWorld は描画後に復元するワールド行列
    void DrawGhost(CStaticMeshRenderer* body, const Vector3& scale, float rotY,
        int startGX, int startGZ, const Matrix4x4& restoreWorld);

    // 攻撃方向選択時の床警告表示（四近傍＋選択マス）
    void DrawAttackWarningFloor(int gridX, int gridZ, Direction attackDir);

    // 攻撃対象のノックバックプレビュー（最前面表示）
    void DrawAttackWarningOverlay(int gridX, int gridZ, Direction attackDir, bool isPush, Unit* self);

    // 行動範囲の発光リングを地面に描画（中心・半径）
    void DrawActionCircle(const Vector3& center, float radius, const Color& color);
    void DrawActionCircleLine(const Vector3& center, float radius, const Color& color);
    void DrawAimWarningBox(const Vector3& center, float yaw, float size, const Color& color);

private:
    float CalculateLineRotation(int dx, int dz);
    float CalculateCornerRotation(int dx1, int dz1, int dx2, int dz2);

    GameContext* m_context = nullptr;   // 非所有（マップ座標変換に使用）
    CStaticMeshRenderer* m_pathLineRenderer = nullptr;
    CStaticMeshRenderer* m_pathCornerRenderer = nullptr;
    CStaticMeshRenderer* m_ringRenderer = nullptr;    // プロシージャル生成の円環（アニュラス）
    CShader* m_fxShader = nullptr; // 無光照シェーダ（発光リング用）
    CStaticMeshRenderer* m_ringLineRenderer = nullptr;
    CStaticMeshRenderer* m_warnBoxRenderer = nullptr;
};