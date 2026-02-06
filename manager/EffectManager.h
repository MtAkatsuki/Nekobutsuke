#pragma once
#include "../system/CSprite.h"
#include <vector>
#include <memory>
#include <random>

class GameContext;

// 瓦礫パーティクルの構造体
struct RubbleParticle {
    bool active = false;
    Vector2 pos;        // スクリーン座標
    Vector2 velocity;   // 速度
    float rotation;     // 回転角度
    float rotSpeed;     // 回転速度
    float scale;
    float lifeTime;     // 残り寿命
    int textureIndex;   // 使用するテクスチャのインデックス (0-3)
};

class EffectManager {
public:
    void Init(GameContext* context);
    void Update(float dt);
    void Draw();
    void Clear();

    // 指定したワールド座標に瓦礫エフェクトを発生させる
    // worldPos: 3D座標（味方の足元など）
    // count: 発生させるパーティクルの数
    void SpawnRubble(const Vector3& worldPos, int count = 3);

private:
    GameContext* m_context = nullptr;
    // プリロードされた4種類の瓦礫テクスチャ
    std::vector<std::unique_ptr<CSprite>> m_textures;
    // パーティクルプール
    std::vector<RubbleParticle> m_particles;

    // 乱数生成器
    std::mt19937 m_gen;
};