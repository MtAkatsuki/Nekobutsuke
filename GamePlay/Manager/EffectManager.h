#pragma once

#include <vector>
#include <memory>
#include <random>
#include "../../System/CSprite.h"

class GameContext;

// =========================================================
// パーティクルの種類
// 物理シミュレーションを伴うものと、純粋なアニメーションを区別する
// =========================================================
enum class ParticleType {
    RUBBLE,     // 瓦礫・破片（重力の影響を受け、放物線を描いて落下・バウンドする）
    HIT_EFFECT  // ヒットエフェクト（指定座標に留まり、スケールのみがアニメーションする）
};

// =========================================================
// 汎用エフェクトパーティクル構造体
// =========================================================
struct EffectParticle {
    bool active = false;
    ParticleType type = ParticleType::RUBBLE;

    Vector2 pos;               // スクリーン座標系の位置
    Vector2 velocity;          // 移動速度 (px/sec)

    float rotation = 0.0f;     // 現在の回転角度 (ラジアン)
    float rotSpeed = 0.0f;     // 回転速度
    float scale = 1.0f;        // 描画スケール

    float lifeTime = 0.0f;     // 残り寿命 (秒)
    float maxLifeTime = 1.0f;  // 初期設定寿命（イージング計算の分母として使用）

    int textureIndex = 0;      // 描画に使用するテクスチャの配列インデックス
};

// =========================================================
// EffectManager クラス
// 2Dスクリーン座標系でのパーティクルエフェクトの生成・更新・描画を一括管理する
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
    // エフェクト生成 (Spawning Interfaces)
    // ---------------------------------------------------------
    // 物理挙動を持つ瓦礫パーティクルを複数生成する（壁衝突や採掘時など）
    void SpawnRubble(const Vector3& worldPos, int count = 3);

    // 打撃の瞬間を示すヒットエフェクトを生成する（攻撃命中時）
    void SpawnHitEffect(const Vector3& worldPos);

    // ---------------------------------------------------------
    // レンダリングパイプライン (Rendering Sub-routines)
    // GameSceneのZオーダー順序に従って個別に呼び出される描画関数群
    // ---------------------------------------------------------
    // 描画順: 5.2 半透明エンティティレイヤー（キャラクターや壁の間に描画）
    void DrawRubble();

    // 描画順: 7 最前面UI・エフェクトレイヤー（UIなどの最前面に重なるように描画）
    void DrawHitEffects();

    // 攻撃プレビュー表示時、障害物となるマスに静的な警告エフェクトを描画する
    void DrawStaticHitPreview(const Vector3& worldPos);

private:
    // =========================================================
    // メンバー変数 (Member Variables)
    // =========================================================
    GameContext* m_context = nullptr;

    // プリロードされたエフェクト用テクスチャ（スプライト）群
    std::vector<std::unique_ptr<CSprite>> m_textures;

    // パーティクルのオブジェクトプール（実行時の動的なメモリ確保を抑えるためのコンテナ）
    std::vector<EffectParticle> m_particles;

    // ランダムな飛び散り方向・速度を決定するための乱数生成器
    std::mt19937 m_gen;
};