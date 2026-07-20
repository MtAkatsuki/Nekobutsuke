#pragma once

#include <vector>
#include <memory>
#include "../../System/CSprite.h"
class CStaticMeshRenderer;
class CShader;
class GameContext;


// =========================================================
// 3D パーティクル（ワールド空間・box メッシュ共用・toon 描画）
// =========================================================
struct Particle3D {
    bool active = false;
    Vector3 pos;                // ワールド座標
    Vector3 velocity;           // ワールド速度 (unit/sec)
    Vector3 rotation;           // オイラー角
    Vector3 rotSpeed;           // 回転速度
    Vector3 baseScale = Vector3(0.15f, 0.15f, 0.15f); // 形状（非等方で破片/スパークを表現）
    Color   color = Color(1, 1, 1, 1);
    float   gravity = 0.0f;     // 個別重力（トレイル等は 0）
    float   life = 1.0f;
    float   maxLife = 1.0f;
    bool    bounce = false;     // 地面バウンドの有無（瓦礫用）
    float   groundY = 0.0f;     // バウンド基準面
    bool    isStar = false;     // 十字スター（ビルボード・拡大→縮小）
    bool    useShard = false;        // fx_shard（三角形）で描画するか（false = cube）
    bool    alignToVelocity = false; // 長手(+X)を速度方向へ向けるか（スパーク用）
};

// =========================================================
// EffectManager クラス
// ワールド空間 3D パーティクルの生成・更新・描画を一括管理する。
// パラメータは System/FxTunables.h（Fx::）に集約。
// =========================================================
class EffectManager {
public:
    EffectManager() = default;
    ~EffectManager() = default;

    // ---------------------------------------------------------
    // ライフサイクルと更新 (Lifecycle & Update)
    // ---------------------------------------------------------
    void Init(GameContext* context);
    void Update(float dt);
    void Clear(); // シーン遷移時やリセット時に全パーティクルを安全に破棄

    // ---------------------------------------------------------
    // レンダリング (Rendering)
    // ---------------------------------------------------------
    // ワールド空間描画（GameScene の透過レイヤーから呼ぶ）
    void Draw3D();

    // 攻撃プレビュー表示時、障害物となるマスに静的な警告アイコンを描画する（2D遺産で唯一存続）
    void DrawStaticHitPreview(const Vector3& worldPos);


    // ---------------------------------------------------------
    // エフェクト生成 (Spawning Interfaces)
    // ---------------------------------------------------------
    void Spawn3DRubble(const Vector3& worldPos, int count = 6); // 採掘・壁衝突の土塊
    void Spawn3DHit(const Vector3& worldPos);                   // 打撃スパーク（放射）
    void Spawn3DDeathBurst(const Vector3& worldPos);            // 死亡時の多色バースト
    void Spawn3DTrailPuff(const Vector3& worldPos);             // 飛翔トレイル（白い残気）
    void Spawn3DStarCross(const Vector3& worldPos);             // 消滅の十字スター

private:
    // =========================================================
    // メンバー変数 (Member Variables)
    // =========================================================
    GameContext* m_context = nullptr;

    // --- 3D パーティクル ---
    std::vector<Particle3D> m_particles3d;
    CStaticMeshRenderer* m_boxRenderer = nullptr;  // 全パーティクル共用の box メッシュ
    CStaticMeshRenderer* m_shardRenderer = nullptr; // 三角形カケラ（スパーク/スター用、遅延取得）
    CShader* m_fxShader = nullptr; 

    // --- 攻撃プレビュー警告アイコン（スクリーン空間スプライト） ---
    std::unique_ptr<CSprite> m_hitPreviewSprite;

};