#pragma once
#include "../../Types/Direction.h"

class GameContext;

// ハードウェアに依存しないゲーム操作の意味的な入力コマンド
struct PlayerCommand {
    bool menuMove = false;
    bool menuAttack = false;
    bool menuEnd = false;
    bool submit = false;
    bool cancel = false;
    DirOffset worldDir = { 0, 0 }; // カメラ方向を考慮して変換済みのワールド座標系の移動方向
};

// ハードウェア入力をゲーム内コマンドへ変換するコントローラ
class PlayerController {
public:
    PlayerController(GameContext* context) : m_context(context) {}
    PlayerCommand PollInput() const;

private:
    GameContext* m_context;
};