#pragma once

// --- ダイアログの種類 ---
enum class DialogueType {
    Help,   // 助けを求める（通常のフェーズ）
    Escape  // 脱出の指示（ターン切れ・脱出開始時）
};