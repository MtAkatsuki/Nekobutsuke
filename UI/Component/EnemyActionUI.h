#pragma once
#include "../../System/CSprite.h"
#include <vector>
#include <memory>

class GameContext;

// =========================================================
// EnemyActionUI クラス
// 敵の頭上に表示される行動順（1, 2, 3...）のUI。
// 登場時のバウンスや待機時の周期的なシェイクアニメーションを管理。
// =========================================================
class EnemyActionUI {
public:
    // ---------------------------------------------------------
    // ライフサイクル (Lifecycle)
    // ---------------------------------------------------------
    void Init(GameContext* context);
    void Update(float dt);

    // ---------------------------------------------------------
    // レンダリング (Rendering)
    // ---------------------------------------------------------
    // order: 行動順 (1から開始)
    void Draw(const Vector3& worldPos, int order);

private:
    GameContext* m_context = nullptr;
    std::vector<std::unique_ptr<CSprite>> m_numSprites;

    // --- アニメーション状態機 ---
    enum class AnimState {
        Entrance,   // 登場（拡大しながら出現）
        Idle,       // 待機
        Shake       // 周期的な揺れ（アテンション誘導）
    };

    AnimState m_state = AnimState::Entrance;

    // アニメーション進行用タイマー
    float m_animTimer = 0.0f;
    float m_intervalTimer = 0.0f;

    // 描画用のトランスフォームキャッシュ（Updateで計算し、Drawで適用）
    float m_currentScale = 1.0f;
    float m_currentRotationZ = 0.0f;
};