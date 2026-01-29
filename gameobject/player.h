#pragma once

#include	<memory>
#include	"../gameobject/Unit.h"
#include	"../system/CStaticMesh.h"
#include	"../system/CStaticMeshRenderer.h"
#include	"../system/CShader.h"
#include	"../system/Direction.h"
#include	"../manager/TurnManager.h"

class MapManager;
class Enemy;

enum class PlayerState {
	MENU_MAIN,          // メインメニュー (J/K/Space)
	MOVE_SELECT,        // 移動操作中 (WASDで)
	ANIM_MOVE,          // 移動アニメション中
	MENU_ATTACK,        // 攻撃メニュー (1/2)
	ATTACK_DIR_SELECT,  // 攻撃方向選択中 (WASDで)
	ANIM_ATTACK,        // 攻撃アニメション中
	WAITING             
};

enum class AttackType {
	Normal,
	Push
};

class Player : public Unit {

public:
	//Unit(GameContext*)継承する
	using Unit::Unit;

	void init() override {}
	void dispose() override {}

	virtual void Update(uint64_t delta) override;
	void OnDraw(uint64_t delta) override;
	void Init();


	// 動きのパラメータ
	//const float VALUE_ROTATE_MODEL = PI * 0.02f;			// 回転速度
	//const float RATE_ROTATE_MODEL = 0.40f;					// 回転慣性係数
	//const float RATE_MOVE_MODEL = 0.20f;					// 移動慣性係数


	void playerResourceLoader();

	int GetCurrentMovePoints() const { return m_currentMovePoints; }

	void HandleInput();
	void AttackInput();
	/*void CameraModeInput();*/
	PlayerState GetState() const { return m_state; }

	int GetPreviewGridX() const { return m_previewGridX; }
	int GetPreviewGridZ() const { return m_previewGridZ; }


private:
	//状態遷移関数
	void SwitchToMenuMain();
	void SwitchToMoveSelect();
	void SwitchToAttackMenu();
	void SwitchToAttackDirSelect(AttackType type);
	void ExecuteMove();
	void ExecuteAttack();
	//入力処理関数
	void HandleMenuInput();
	void HandleMoveInput(float dt);
	void HandleAttackMenuInput();
	void HandleAttackDirInput(float dt);
	//描画補助関数
	void DrawGhost();       // スタートポイントのプレーヤーのゴーストを描画
	void DrawPathLine();    // 移動予想ルートを描画
	void DrawAttackWarning();// 攻撃予想範囲を描画

	void PlayerStartMoveTo(int targetX, int targetZ);
	virtual void StartTurn()override;
	virtual void EndTurn()override;
	void PullAttack(GameContext* context);
	void PushAttack();
	virtual void TakeDamage(int damage, Unit* attacker)override;

	virtual void OnTurnChanged(TurnState state) override;
	Unit* GetTargetInLine(int range);

	void HandleMoveSelectionInput(float dt);//移動入力
	void UpdatePathToPrivew();//スタート点からゴールまでのルート

	bool UpdatePathMovement(float dt);
	/*void UpdatePaperOrientation();*/


private:
	enum class ControlState 
	{
		SELECT_MOVE,//移動操作中、移動範囲と移動予想位置を表示する
		ANIMATING,//移動アニメーション中
		WAITING//プレーヤー以外のターン
	};
	ControlState m_controlState = ControlState::WAITING;

	//CStaticMesh*			m_PlayerMesh;
	//CStaticMeshRenderer*	m_PlayerMeshrenderer;
	CShader*	m_PlayerShader;
	//状態
	PlayerState m_state = PlayerState::WAITING;
	bool canControl = false;

	//移動データ
	int m_startGridX = 0;   // スタートポイントのX
	int m_startGridZ = 0;   // スタートポイントのZ
	int m_previewGridX = 0; // 操作中の予想移動先のX
	int m_previewGridZ = 0; // 操作中の予想移動先のZ
	float m_inputCooldown = 0.0f;
	std::vector<Tile*> m_moveRangeTiles; // 緑の移動範囲タイル
	std::vector<Tile*> m_currentPath;

	//攻撃データ
	AttackType m_selectedAttackType = AttackType::Normal;
	Direction m_attackDir = Direction::North; // 攻撃方向

	bool m_hasActioned = false;
	bool m_isRotating = false;
	bool m_hasMoved = false;
	
	// 移動アニメション用データ
	int m_pathAnimIndex = 0; //現在はどのパスを移動中か
	const float MOVE_SPEED = 10.0f; // 移動スピード

	// パス描画用メッシュレンダラー
	CStaticMeshRenderer* m_PathLineRenderer = nullptr;   // 直線 (Left -> Right)
	CStaticMeshRenderer* m_PathCornerRenderer = nullptr; // 直角 (Left -> Bottom)

	// パスの回転計算
	float CalculateLineRotation(int dx, int dz);
	float CalculateCornerRotation(int dx1, int dz1, int dx2, int dz2);
	// 次のメニュー状態(アニメション実行を確保するため)
	PlayerState m_nextState = PlayerState::WAITING;
};