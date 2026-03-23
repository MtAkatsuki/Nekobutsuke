#pragma once
#include "../../System/CSprite.h"
#include <memory>

class GameContext;

// =========================================================
// HPBar クラス
// キャラクターの頭上に表示されるHPバーUI。
// ダメージプレビュー時の点滅アニメーション制御を含む。
// =========================================================
class HPBar {
public:
    enum class BarType {
        Green, // プレイヤー用
        Red    // エネミー用
    };

    HPBar() = default;
    ~HPBar() = default;

    // ---------------------------------------------------------
    // ライフサイクル (Lifecycle)
    // ---------------------------------------------------------
    void Init(GameContext* context);
    void Update(float dt);

    // ---------------------------------------------------------
    // レンダリング (Rendering)
    // ---------------------------------------------------------
    // previewDamage: ダメージのプレビュー量（右から左へ点滅させるダメージ予定分）
    void Draw(const Vector3& worldPos, int currentHP, int maxHP, int previewDamage = 0);

public:
    // --- ImGui 調整用パラメータ ---
    // 外部（エディタ等）から動的に調整できるようpublic静的変数として公開
    static float s_hpBarOffsetY;
    static float s_hpBarTexSize;
    static float s_hpBarGap;

private:
    GameContext* m_context = nullptr;

    // --- UI アセット ---
    std::unique_ptr<CSprite> m_frameSprite; // 底枠（空のハート）
    std::unique_ptr<CSprite> m_fullSprite;  // 赤いハート（中身）

    // --- アニメーション制御 ---
    float m_blinkTimer = 0.0f;
    bool  m_isBlinkOn = true; // 点滅時の表示ON/OFF状態
};