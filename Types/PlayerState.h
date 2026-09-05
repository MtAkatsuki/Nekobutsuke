#pragma once

// =========================================================
// PlayerState
// プレイヤーのステートマシン定義
// =========================================================
enum class PlayerState {
    MENU_MAIN,          // 【撤去予定】旧メインメニュー
    MOVE_SELECT,        // 【撤去予定】旧・格子移動
    ANIM_MOVE,          // 【撤去予定】旧・移動アニメ
    ATTACK_DIR_SELECT,  // 【撤去予定】旧・攻撃方向選択
    ANIM_ATTACK,
    ANIM_ATTACK_WINDUP,
    WAITING,
    DEAD_FLYING,
    ANIM_CELEBRATE,
    KNOCKBACK,
    AIM,                // 攻撃モード：敵をロックし、判定円で命中を狙う（本モジュール）
    FREE_MOVE,          // 三人称：連続ドライブ（本モジュールで追加）
};