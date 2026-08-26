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

    // 3. 三人称：カメラ方位を基準に前後左右をワールドへ写像
    if (screenDir.x != 0 || screenDir.z != 0) {
        float a = 0.0f;
        if (m_context && m_context->GetCamera()) a = m_context->GetCamera()->GetAzimuth();

        // カメラ前方（地面投影） F=(cos a, sin a)、右 R=(sin a, -cos a)
        const float fwdX = cosf(a), fwdZ = sinf(a);
        const float rgtX = sinf(a), rgtZ = -cosf(a);

        // screenDir.z=前後（W で F 方向へ）, screenDir.x=左右（D で R 方向へ）
        const float wx = fwdX * screenDir.z + rgtX * screenDir.x;
        const float wz = fwdZ * screenDir.z + rgtZ * screenDir.x;

        // グリッド移動のため、最も近い四方位へスナップ
        if (fabsf(wx) > fabsf(wz)) cmd.worldDir = { (wx > 0.0f) ? 1 : -1, 0 };
        else                       cmd.worldDir = { 0, (wz > 0.0f) ? 1 : -1 };
    }

    return cmd;
}