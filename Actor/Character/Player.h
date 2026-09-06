#pragma once

#include <memory>
#include "PlayerController.h"
#include "../Base/Unit.h"
#include "../../System/CStaticMesh.h"
#include "../../System/CStaticMeshRenderer.h"
#include "../../System/CShader.h"
#include "../../Types/Direction.h"
#include "../../GamePlay/Manager/TurnManager.h"
#include "../../Types/PlayerState.h"
#include "../../Types/AttackType.h"
#include "../../System/collision.h"

class MapManager;
class Enemy;
class PlayerActionView;


// =========================================================
// Player クラス
// プレイヤー操作のインターフェースと、状態遷移（ステートマシン）を担う
// =========================================================
class Player : public Unit {
public:
    
    // ---------------------------------------------------------
    // ライフサイクル (Lifecycle)
    // ---------------------------------------------------------
    static std::unique_ptr<Player> Spawn(GameContext* ctx, int gridX, int gridZ, const Vector3& worldPos);

    void Dispose() override {}
    virtual void Update(float deltaSeconds) override;
    void OnDraw(float deltaSeconds) override;
    ~Player() override;

    // ---------------------------------------------------------
    // 状態とクエリ (Status & Queries)
    // ---------------------------------------------------------
    PlayerState GetState() const { return m_state; }
    int GetCurrentMovePoints() const { return m_currentMovePoints; }
    bool IsCelebrationDone() const { return m_isCelebrationDone; }
    void SetMenuHold(bool h) { m_menuHold = h; }   //カメラ帰還＋UI再生中は GameScene 側でメニュー操作を無効化

    // ---------------------------------------------------------
    // フローとイベント (Flow & Events)
    // ---------------------------------------------------------
    // 勝利演出（ジャンプ）を開始する
    void StartCelebration();
    virtual void SetPreviewDamage(int dmg) override;
    virtual void OnDeathFlyComplete() override;
    // 外部からコマンドを注入し、入力処理とゲームロジックを分離
    void SetCommand(const PlayerCommand& cmd) { m_currentCmd = cmd; }

    // ---------------------------------------------------------
	// Debug / テスト用 (Debug / Testing)
    // ---------------------------------------------------------
    void DebugForceAttack(Direction dir, AttackType type = AttackType::Push);


    virtual void DrawUI() override;   // 玩家HP + 攻撃モードのロックHUD

protected:
    using Unit::Unit;

    virtual void StartTurn() override;
    virtual void EndTurn() override;
    virtual void TakeDamage(int damage, Unit* attacker) override;
    virtual void Die();
    virtual void OnTurnChanged(TurnState state) override;

    virtual void OnDrawFloorUI(float deltaSeconds) override;
    virtual void OnDrawTransparent(float deltaSeconds) override;
    virtual void OnDrawOverlay(float deltaSeconds) override;

    void OnKnockbackBegin() override;
    void OnKnockbackEnd() override;

private:
    void Init();
    void LoadPlayerResources();

    // --- 状態遷移 (State Transitions) ---
    // ウィンドアップ後の実際の打撃。ダメージと押し出しを適用する
    void PerformAttackStrike();
    // 攻撃開始。カメラ演出とウィンドアップに入る（打撃は PerformAttackStrike）
    void ExecuteAttack();

    // --- 入力ハンドラ (Input Handlers) ---
    // FREE_MOVE：連続ドライブの入力処理（移動・攻撃入口・ターン終了）
    void HandleFreeMove(float dt);
    // 移動を 1 フレーム進める（壁衝突＋行動円クランプ＋連続 yaw）。動いたら true
    bool DriveContinuous(const Vector3& worldDir, float dt);

    // --- ユーティリティ (Utilities) ---
    // 勝利ジャンプ演出を更新する
    void UpdateCelebration(float dt);

    void UpdateTrapPreview(); // 足元の罠ダメージを毎フレーム予測表示

    // 攻撃モード
    void EnterAim();                 // 右クリックで進入：最寄り敵をロック＋構図
    void ExitAim();                  // 右クリックで退出：FREE_MOVE へ戻す
    void HandleAim(float dt);        // AIM 中：移動可・QE切替・構図追従・確定
    void SelectNearestEnemy();       // 最寄りの攻撃可能な敵を m_aimTarget に
    void CycleTarget(int step);      // Q(-1)/E(+1) でロック対象を切替
    void GetAimBox(Vector3& center, float& yaw) const;  // 予警区の中心・向きを算出（判定と描画で共用）
    bool IsAimHit() const;                              // 予警区(OBB) ∩ 敵受击円(sphere)
    void DrawAimHUD();   // 敵両脇のロック箭头 + 赤X（2Dスクリーン）

    // =========================================================
    // メンバー変数
    // =========================================================
    CShader* m_playerShader = nullptr;

    PlayerState m_state = PlayerState::WAITING;
    bool canControl = false;
    bool m_menuHold = false;
    // 現在フレームの操作コマンドを保持
    PlayerCommand m_currentCmd;

    // --- 移動 ---
    float   m_actionRadius = 4.0f;   // 行動可能円の半径（＝移動力パラメータ）
    void SyncGridFromWorld();   // 連続移動中、論理格 m_gridX/Z を現在位置から更新（発火しない）

    // --- 戦闘・攻撃 ---
    int m_playerDamage = 1;
    float m_attackWindupTimer = 0.0f;
    class Unit* m_aimTarget = nullptr;   // 現在ロックしている敵
    bool m_canAimHit = false;   // 予警区が敵の受击円と重なっているか
    Vector3 m_attackPushDir;

    // --- フラグ ---
    bool m_isZoomedIn = false;
    bool m_isWaitingTurnStart = false;
    bool m_attackIsLethal = false;
    bool m_killedByHazard = false;   // 直近の致死が無攻撃者（罠など）か
    float m_deathFlyDelay = 0.0f;

    // --- 勝利演出 ---
    int m_jumpCount = 0;
    float m_jumpTimer = 0.0f;
    bool m_isCelebrationDone = false;

    // --- 描画用 ---
    std::unique_ptr<PlayerActionView> m_actionView;
    std::unique_ptr<CSprite> m_aimArrowSprite;   // ロック箭头（敵両脇）
    std::unique_ptr<CSprite> m_aimCrossSprite;   // 攻撃不可の赤X
    float m_aimArrowAnimTimer = 0.0f;            // 箭头の弾動アニメ用
};