#pragma once

// =========================================================
// PlayerState
// プレイヤーのステートマシン定義
// =========================================================
enum class PlayerState {
    ANIM_ATTACK,
    ANIM_ATTACK_WINDUP,
    WAITING,
    DEAD_FLYING,
    ANIM_CELEBRATE,
    KNOCKBACK,
    AIM,                // 攻撃モード：敵をロックし、判定円で命中を狙う（本モジュール）
    FREE_MOVE,          // 三人称：連続ドライブ（本モジュールで追加）
};