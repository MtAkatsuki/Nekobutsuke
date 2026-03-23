#pragma once
#include "../../System/CSprite.h"
#include <memory>

class GameContext;

// --- ダイアログの種類 ---
enum class DialogueType {
    Help,   // 助けを求める（通常のフェーズ）
    Escape  // 脱出の指示（ターン切れ・脱出開始時）
};

// =========================================================
// DialogueUI クラス
// ユニットの頭上に一時的に表示される吹き出しUIを管理。
// =========================================================
class DialogueUI {
public:
    DialogueUI() = default;
    ~DialogueUI() = default;

    // ---------------------------------------------------------
    // ライフサイクル (Lifecycle)
    // ---------------------------------------------------------
    void Init(GameContext* context);
    void Update(uint64_t delta);

    // ---------------------------------------------------------
    // レンダリング (Rendering)
    // ---------------------------------------------------------
    void Draw();

    // ---------------------------------------------------------
    // フロー制御 (Flow)
    // ---------------------------------------------------------
    // duration: 表示時間（負数の場合は無限表示）
    void ShowDialogue(const Vector3& targetWorldPos, DialogueType type, float duration = 3.0f);
    void HideDialogue();

    bool IsShowing() const { return m_isVisible; }

private:
    GameContext* m_context = nullptr;

    // --- UI アセット ---
    std::unique_ptr<CSprite> m_dialogueHelpSprite;
    std::unique_ptr<CSprite> m_dialogueEscapeSprite;
    CSprite* m_currentSprite = nullptr;

    // --- 状態管理 ---
    bool m_isVisible = false;
    float m_displayTimer = 0.0f;
    Vector3 m_targetPos{ 0.0f, 0.0f, 0.0f }; // 追従対象のベースワールド座標
};