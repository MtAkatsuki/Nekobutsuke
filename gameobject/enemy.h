#pragma once

#include	<memory>
#include	"Unit.h"
#include	"../system/CAnimationObject.h"
#include	"../system/CAnimationMesh.h"
#include	"../system/CAnimationdata.h"
#include	"../system/CShader.h"
#include	"../system/IScene.h"
#include    "../manager/TurnManager.h"
#include	"../ui/EnemyActionUI.h"

class player;


enum class EnemyState {
	IDLE,
	MOVING,
	ATTACKING,
	KNOCKBACK,
	DEAD_FLYING
};


class Enemy : public Unit {

public:
	using Unit::Unit;

	virtual void Update(uint64_t delta) override;
	void OnDraw(uint64_t delta)override;
	void init() override;
	void Init(int sequenceNumber);
	void dispose() override;
	void EnemyStartMoveTo(std::vector<Tile*> path);

	void EnemyStartAction();
	void SetDisplayOrder(int order);
	virtual void DrawUI() override;
	bool IsIdle() const { return m_state == EnemyState::IDLE; }
	void ResetCharge() { 
		m_isCharging = false; 
		m_pendingCharge = false;
	}
	virtual void OnPushed(Direction pushDir) override;//敵のノックバック（押し出し）処理
	virtual void TakeDamage(int damage, Unit* attacker)override;
	bool IsDeadFlying() const { return m_state == EnemyState::DEAD_FLYING; }//死亡飛翔中か

	// プレイヤーの移動予測に敵の攻撃範囲を提供するための関数
	bool IsCharging() const { return m_isCharging; }
	int GetLockedGridX() const { return m_lockedGridX; }
	int GetLockedGridZ() const { return m_lockedGridZ; }

	int GetEnemyDamage() const { return m_enemyDamage; }

private:
	
	void ExecuteAI();
	void EnemyEndAction();
	void ReleaseChargeAttack();


	virtual void OnTurnChanged(TurnState state) override;
	void updateMove(uint64_t delta);
	void onMoveFinished();
	void StartCharge(Unit* target);
	void ChargeAnimation();
	void DeathFlyingUpdate(float delta);



private:
	CStaticMesh*			m_EnemyMesh;
	CStaticMeshRenderer*	m_EnemyMeshrenderer;
	CShader*				m_EnemyShader;
	CStaticMeshRenderer* m_pushArrowRenderer = nullptr;
	CStaticMeshRenderer* m_attackArrowRenderer = nullptr;
	std::unique_ptr<EnemyActionUI> m_actionUI;

	EnemyState m_state = EnemyState::IDLE;

	int m_MovePoints = 3;				// 移動可能ポイント
	int m_enemyDamage = 2;				// 敵の攻撃力

	std::vector<Tile*> m_currentPath;
	int m_pathIndex = 0;
	float m_moveSpeed = 0.1f;
	Vector3 m_targetWorldPos;

	float m_attackTimer = 0.0f; 
	//Unit* m_attackTarget = nullptr; 
	bool m_isMyTurn = false;

	int m_displayOrder = 0;

	int m_lockedGridX = -1;//攻撃準備時のロック　オン
	int m_lockedGridZ = -1;

	bool m_isCharging = false;//チャージ攻撃中のフラグ
	bool m_pendingCharge = false; //チャージ攻撃予約フラグ

	Vector3 m_deathVelocity = Vector3(0, 0, 0);//死亡時の飛翔速度
	Vector3 m_deathSpin = Vector3(0, 0, 0);//死亡時の回転軸

	const float GRAVITY = 50.0f;//重力加速度
	//ペーパーアニメーション用
	float lastScaleX = 1.0f;

	// 敵の移動範囲を保持する変数
	std::vector<Tile*> m_moveRangeTiles;

protected:
	virtual void OnDrawOverlay(uint64_t delta) override;
	virtual void OnDrawFloorUI(uint64_t delta) override;
};