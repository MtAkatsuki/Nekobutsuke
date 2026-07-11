#pragma once

#include <memory>
#include "../Base/Unit.h"
#include "../../System/CStaticMesh.h"
#include "../../System/CStaticMeshRenderer.h"
#include "../../System/CShader.h"
#include "../../System/Direction.h"
#include "../../GamePlay/Manager/TurnManager.h"

class MapManager;
class Enemy;
class PlayerActionView;

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

enum class AttackType {
    Normal,
    Push
};

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
    virtual void Update(uint64_t delta) override;
    void OnDraw(uint64_t delta) override;
    ~Player() override;

    // ---------------------------------------------------------
    // 状態とクエリ (Status & Queries)
    // ---------------------------------------------------------
    PlayerState GetState() const { return m_state; }
    int GetCurrentMovePoints() const { return m_currentMovePoints; }
    int GetPreviewGridX() const { return m_previewGridX; }
    int GetPreviewGridZ() const { return m_previewGridZ; }
    bool IsCelebrationDone() const { return m_isCelebrationDone; }

    // ---------------------------------------------------------
    // フローとイベント (Flow & Events)
    // ---------------------------------------------------------
    void StartCelebration();
    virtual void OnPushed(Direction pushDir, Unit* attacker = nullptr) override;
    virtual void SetPreviewDamage(int dmg) override;
    virtual void OnDeathFlyComplete() override;

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

    virtual void OnDrawFloorUI(uint64_t delta) override;
    virtual void OnDrawTransparent(uint64_t delta) override;
    virtual void OnDrawOverlay(uint64_t delta) override;

private:
    void Init();
    void LoadPlayerResources();

    // --- 状態遷移 (State Transitions) ---
    void SwitchToMenuMain();
    void SwitchToMoveSelect();
    void SwitchToAttackDirSelect(AttackType type);
    void ExecuteMove();
    void PerformAttackStrike();
    void ExecuteAttack();

    // --- 入力ハンドラ (Input Handlers) ---
    void HandleMenuInput();
    void HandleMoveInput(float dt);
    void HandleAttackDirInput(float dt);


    // --- ユーティリティ (Utilities) ---
    bool UpdatePathMovement(float dt);
    void CalculateMovePreviewDamage();
    void UpdateCelebration(float dt);

    // =========================================================
    // メンバー変数
    // =========================================================
    CShader* m_playerShader = nullptr;

    PlayerState m_state = PlayerState::WAITING;
    PlayerState m_nextState = PlayerState::WAITING;
    bool canControl = false;

    // --- 移動・パス ---
    int m_startGridX = 0;
    int m_startGridZ = 0;
    int m_previewGridX = 0;
    int m_previewGridZ = 0;
    float m_inputCooldown = 0.0f;
    std::vector<Tile*> m_moveRangeTiles;
    std::vector<Tile*> m_currentPath;
    int m_pathAnimIndex = 0;

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