#pragma once

// =========================================================
// PlayerState
// プレイヤーのステートマシン定義
// =========================================================
enum class PlayerState {
    MENU_MAIN,          // メインメニュー表示中 (入力待ち)
    MOVE_SELECT,        // 移動モード (WASDでカーソル移動)
    ANIM_MOVE,          // 移動アニメーション進行中
    ATTACK_DIR_SELECT,  // 攻撃方向選択中 (WASDで方向切り替え)
    ANIM_ATTACK,        // 攻撃アニメーション進行中
    ANIM_ATTACK_WINDUP,
    WAITING,            // 敵/味方ターン中の待機状態
    DEAD_FLYING,        // 死亡飛行中 (死亡時の演出)
    ANIM_CELEBRATE,     // クリア時の勝利アニメーション
    KNOCKBACK           // 敵からのノックバック被弾中
};