#pragma once
#include "../../Types/Direction.h"

class GameContext;

// ハードウェアに依存しないゲーム操作の意味的な入力コマンド
struct PlayerCommand {
    // ===== 新（連続化）：三人称アクション用 =====
    // カメラ相対・ワールドXZ・正規化済みの移動ベクトル（大きさ 0 か 1）。
    // 四方位スナップは掛けない＝斜め移動もそのまま通す。
    float moveX = 0.0f;
    float moveZ = 0.0f;

    bool aimToggle = false; // 右クリック：攻撃モードの ON/OFF（意味は状態側で解釈）
    bool attackConfirm = false; // 左クリック：攻撃確定
    bool targetPrev = false; // Q：狙う敵を切り替え（前）
    bool targetNext = false; // E：狙う敵を切り替え（次）
    bool endTurn = false; // ESC：ターン終了

    float aimYawDelta = 0.0f;  // 攻撃モード中の照準回転用：マウス水平移動量（生値・感度は消費側で掛ける）

    // ===== 旧（メニュー式）：Player 状態機の書き換え時に撤去予定 =====
    bool menuMove = false;
    bool menuAttack = false;
    bool menuEnd = false;
    bool submit = false;
    bool cancel = false;
    DirOffset worldDir = { 0, 0 }; // 四方位スナップ済み（旧移動・攻撃方向選択が使用）
};

// ハードウェア入力をゲーム内コマンドへ変換するコントローラ
class PlayerController {
public:
    PlayerController(GameContext* context) : m_context(context) {}
    PlayerCommand PollInput() const;

private:
    GameContext* m_context;
};