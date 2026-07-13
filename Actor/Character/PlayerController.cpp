#include "PlayerController.h"
#include "../../System/CDirectInput.h"
#include "../../System/Camera.h"
#include "../../Core/GameContext.h"

PlayerCommand PlayerController::PollInput() const {
    PlayerCommand cmd;
    auto& input = CDirectInput::GetInstance();

    // 1. ハードウェア入力をゲーム内コマンドへ変換
    cmd.menuMove = input.CheckKeyBufferTrigger(DIK_J);
    cmd.menuAttack = input.CheckKeyBufferTrigger(DIK_K);
    cmd.menuEnd = input.CheckKeyBufferTrigger(DIK_L);
    cmd.submit = input.CheckKeyBufferTrigger(DIK_RETURN);
    cmd.cancel = input.CheckKeyBufferTrigger(DIK_ESCAPE);

    // 2. 画面基準の入力方向を取得
    DirOffset screenDir = { 0, 0 };
    if (input.CheckKeyBuffer(DIK_W) || input.CheckKeyBuffer(DIK_UP))         screenDir = { 0,  1 };
    else if (input.CheckKeyBuffer(DIK_S) || input.CheckKeyBuffer(DIK_DOWN))  screenDir = { 0, -1 };
    else if (input.CheckKeyBuffer(DIK_A) || input.CheckKeyBuffer(DIK_LEFT))  screenDir = { -1, 0 };
    else if (input.CheckKeyBuffer(DIK_D) || input.CheckKeyBuffer(DIK_RIGHT)) screenDir = { 1,  0 };

    // 3. カメラの向きを考慮し、ワールド座標系の方向へ変換
    if (screenDir.x != 0 || screenDir.z != 0) {
        int camDir = (m_context && m_context->GetCamera()) ? m_context->GetCamera()->GetNormalizedDirIndex() : 0;
        switch (camDir) {
        case 1:  cmd.worldDir = screenDir.MoveLeft(); break;   // 正面右カメラ
        case 2:  cmd.worldDir = screenDir.MoveBack(); break;   // 背面右カメラ
        case 3:  cmd.worldDir = screenDir.MoveRight(); break;  // 背面左カメラ
        default: cmd.worldDir = screenDir; break;              // 正面左（基本カメラ）
        }
    }

    return cmd;
}