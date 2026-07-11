#pragma once
#include <vector>
#include <string>
#include <map>
#include <memory>
#include "../../System/CSprite.h"

class GameContext;
class Camera;

// ---------------------------------------------------------
// データ構造体
// ---------------------------------------------------------
struct UVQuad {
    Vector2 tex[4];
};

struct ActiveDamageNumber {
    Vector3 startWorldPos;
    Vector3 currentOffset;
    int number;
    float timer;

    // フェーズごとのタイミング閾値
    float timePhase1;
    float timePhase2;
    float totalTime;
};

// =========================================================
// DamageNumberManager クラス
// 戦闘時のダメージポップアップ数値を管理・描画するシステム。
// カメラの向きに応じた背面カリングと、フェーズ進行によるアルファ/スケール補間を行う。
// =========================================================
class DamageNumberManager {
public:
    DamageNumberManager() = default;
    ~DamageNumberManager() = default;

    // ---------------------------------------------------------
    // ライフサイクル (Lifecycle)
    // ---------------------------------------------------------
    void Init(GameContext* context);
    void Update(uint64_t dt);

    // ---------------------------------------------------------
    // レンダリング (Rendering)
    // ---------------------------------------------------------
    void Draw();

    // ---------------------------------------------------------
    // フロー制御 (Flow)
    // ---------------------------------------------------------
    void SpawnDamage(Vector3 position, int damage);
    void ClearAll() { m_activeNumbers.clear(); }

private:
    GameContext* m_context = nullptr;

    std::unique_ptr<CSprite> m_sprite;
    std::map<char, UVQuad> m_numberUVs;
    std::vector<ActiveDamageNumber> m_activeNumbers;
};