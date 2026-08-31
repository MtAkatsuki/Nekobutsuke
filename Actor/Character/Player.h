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
    int GetPreviewGridX() const { return m_previewGridX; }
    int GetPreviewGridZ() const { return m_previewGridZ; }
    bool IsCelebrationDone() const { return m_isCelebrationDone; }
    void SetMenuHold(bool h) { m_menuHold = h; }   //カメラ帰還＋UI再生中は GameScene 側でメニュー操作を無効化

    // ---------------------------------------------------------
    // フローとイベント (Flow & Events)
    // ---------------------------------------------------------
    // 勝利演出（ジャンプ）を開始する
    void StartCelebration();
    virtual void OnPushed(Direction pushDir, Unit* attacker = nullptr) override;
    virtual void SetPreviewDamage(int dmg) override;
    virtual void OnDeathFlyComplete() override;
    // 外部からコマンドを注入し、入力処理とゲームロジックを分離
    void SetCommand(const PlayerCommand& cmd) { m_currentCmd = cmd; }

    // ---------------------------------------------------------
	// Debug / テスト用 (Debug / Testing)
    // ---------------------------------------------------------
    void DebugForceAttack(Direction dir, AttackType type = AttackType::Push);

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

private:
    void Init();
    void LoadPlayerResources();

    // --- 状態遷移 (State Transitions) ---
    // メインメニューへ遷移。移動・攻撃の可否を判定して UI を開く
    void SwitchToMenuMain();
    // 移動選択へ遷移。到達可能マスを算出してガイド UI を表示する
    void SwitchToMoveSelect();
    // 攻撃方向選択へ遷移。カメラを攻撃方向へ寄せる
    void SwitchToAttackDirSelect(AttackType type);
    // 選択した経路に沿った移動アニメーションを開始する
    void ExecuteMove();
    // ウィンドアップ後の実際の打撃。ダメージと押し出しを適用する
    void PerformAttackStrike();
    // 攻撃開始。カメラ演出とウィンドアップに入る（打撃は PerformAttackStrike）
    void ExecuteAttack();

    // --- 入力ハンドラ (Input Handlers) ---
    // メインメニューのキー入力を次状態への遷移要求に変換する
    void HandleMenuInput();
    // 移動選択中の入力処理。カーソル移動・経路更新・確定を行う
    void HandleMoveInput(float dt);
    // 攻撃方向選択中の入力処理。方向変更・カメラ追従・確定を行う
    void HandleAttackDirInput(float dt);
    // FREE_MOVE：連続ドライブの入力処理（移動・攻撃入口・ターン終了）
    void HandleFreeMove(float dt);
    // 移動を 1 フレーム進める（壁衝突＋行動円クランプ＋連続 yaw）。動いたら true
    bool DriveContinuous(const Vector3& worldDir, float dt);
    // pos を行動円（中心 m_moveStartPos・半径 m_actionRadius）内へ丸めた位置を返す
    Vector3 ClampToActionCircle(const Vector3& pos) const;

    // --- ユーティリティ (Utilities) ---
    // 移動アニメーションを 1 フレーム進める（完了で true）
    bool UpdatePathMovement(float dt);
    // 移動先で受ける罠・敵ロックオンの予測ダメージを算出する
    void CalculateMovePreviewDamage();
    // 勝利ジャンプ演出を更新する
    void UpdateCelebration(float dt);

    // =========================================================
    // メンバー変数
    // =========================================================
    CShader* m_playerShader = nullptr;

    PlayerState m_state = PlayerState::WAITING;
    PlayerState m_nextState = PlayerState::WAITING;
    bool canControl = false;
    bool m_menuHold = false;
    // 現在フレームの操作コマンドを保持
    PlayerCommand m_currentCmd;

    // --- 移動・パス ---
    int m_startGridX = 0;
    int m_startGridZ = 0;
    int m_previewGridX = 0;
    int m_previewGridZ = 0;
    float m_inputCooldown = 0.0f;
    std::vector<Tile*> m_moveRangeTiles;
    std::vector<Tile*> m_currentPath;
    int m_pathAnimIndex = 0;
    float   m_actionRadius = 4.0f;   // 行動可能円の半径（＝移動力パラメータ）
    Vector3 m_moveStartPos;          // 行動円の中心（ターン開始位置）

    // --- 戦闘・攻撃 ---
    AttackType m_selectedAttackType = AttackType::Normal;
    Direction m_attackDir = Direction::North;
    int m_playerDamage = 1;
    bool m_canAttack = false;
    float m_attackWindupTimer = 0.0f;

    // --- フラグ ---
    bool m_hasMoved = false;
    bool m_isZoomedIn = false;
    bool m_isWaitingTurnStart = false;
    bool  m_attackIsLethal = false;
    bool  m_isDebugAttack = false;

    // --- 勝利演出 ---
    int m_jumpCount = 0;
    float m_jumpTimer = 0.0f;
    bool m_isCelebrationDone = false;

    // --- 描画用 ---
    std::unique_ptr<PlayerActionView> m_actionView;
};