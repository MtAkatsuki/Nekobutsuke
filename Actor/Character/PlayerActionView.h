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

    // 攻撃モードの矩形警告エリア（AIM で使用）
    void DrawAimWarningBox(const Vector3& center, float yaw, float size, const Color& color);

private:
    GameContext* m_context = nullptr;   // 非所有
    CShader* m_fxShader = nullptr;      // 無光照シェーダ
    CStaticMeshRenderer* m_warnBoxRenderer = nullptr;
};