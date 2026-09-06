#include "PlayerController.h"
#include "../../System/CDirectInput.h"
#include "../../System/Camera.h"
#include "../../Core/GameContext.h"

PlayerCommand PlayerController::PollInput() const {
    PlayerCommand cmd;
    auto& input = CDirectInput::GetInstance();

    // =========================================================
    // 1. 旧メニュー系（撤去予定・当面は互換のため残す）
    // =========================================================
    cmd.menuMove = input.CheckKeyBufferTrigger(DIK_J);
    cmd.menuAttack = input.CheckKeyBufferTrigger(DIK_K);
    cmd.menuEnd = input.CheckKeyBufferTrigger(DIK_L);
    cmd.submit = input.CheckKeyBufferTrigger(DIK_RETURN);
    cmd.cancel = input.CheckKeyBufferTrigger(DIK_ESCAPE);

    // =========================================================
// 2. 連続移動：スクリーン基準の入力を「加算」で集計（八方向対応）
//    ※ if / else if では同時押し（斜め）が拾えないため加算にする
// =========================================================
    float sx = 0.0f; // 右(+) / 左(-)
    float sz = 0.0f; // 前(+) / 後(-)
    if (input.CheckKeyBuffer(DIK_W) || input.CheckKeyBuffer(DIK_UP))    sz += 1.0f;
    if (input.CheckKeyBuffer(DIK_S) || input.CheckKeyBuffer(DIK_DOWN))  sz -= 1.0f;
    if (input.CheckKeyBuffer(DIK_A) || input.CheckKeyBuffer(DIK_LEFT))  sx -= 1.0f;
    if (input.CheckKeyBuffer(DIK_D) || input.CheckKeyBuffer(DIK_RIGHT)) sx += 1.0f;

    if (sx != 0.0f || sz != 0.0f) {
        // カメラ方位を取得（無ければ 0）
        float a = 0.0f;
        if (m_context && m_context->GetCamera()) a = m_context->GetCamera()->GetAzimuth();

        // カメラ前方（地面投影） F=(cos a, sin a)、右 R=(sin a, -cos a)
        const float fwdX = cosf(a), fwdZ = sinf(a);
        const float rgtX = sinf(a), rgtZ = -cosf(a);

        // sz=前後（W で F 方向へ）, sx=左右（D で R 方向へ）をワールドへ合成
        float wx = fwdX * sz + rgtX * sx;
        float wz = fwdZ * sz + rgtZ * sx;

        // 正規化：斜め入力（大きさ√2）でも速度が増えないように単位ベクトル化
        float len = sqrtf(wx * wx + wz * wz);
        if (len > 0.0001f) { wx /= len; wz /= len; }

        cmd.moveX = wx;
        cmd.moveZ = wz;

        // 旧コード互換：四方位スナップ版も引き続き埋める（攻撃方向選択などが使用）
        if (fabsf(wx) > fabsf(wz)) cmd.worldDir = { (wx > 0.0f) ? 1 : -1, 0 };
        else                       cmd.worldDir = { 0, (wz > 0.0f) ? 1 : -1 };
    }

    // =========================================================
    // 3. モード操作：エッジtrigger（押した瞬間だけ true）
    //    切替・確定は 1 回だけ効けばよいので hold ではなく trigger
    // =========================================================
    // s_ignoreMouseLook 中は左右クリックをゲーム側で消費しない（ImGui のみが読む）
    if (!Camera::s_ignoreMouseLook) {
        cmd.aimToggle = input.GetMouseRButtonTrigger();
        cmd.attackConfirm = input.GetMouseLButtonTrigger();
    }
    // 無視中は false のまま
    cmd.targetPrev = input.CheckKeyBufferTrigger(DIK_Q);
    cmd.targetNext = input.CheckKeyBufferTrigger(DIK_E);
    cmd.endTurn = input.CheckKeyBufferTrigger(DIK_ESCAPE);

    // =========================================================
    // 4. 照準回転用のマウス水平移動量（攻撃モード中のみ消費側で使う）
    // =========================================================
    if (!Camera::s_ignoreMouseLook) {
        cmd.aimYawDelta = static_cast<float>(input.GetMouseMoveX());
        cmd.aimPitchDelta = static_cast<float>(input.GetMouseMoveY());
    }
    // 無視中は 0 のまま（AimFollow 側は「入力なし」として扱い、帰位が働く）
    return cmd;
}