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
	ATTACK_DIR_SELECT,  // 攻撃方向選択中 (WASDで)
	ANIM_ATTACK,        // 攻撃アニメション中
	WAITING,
	ANIM_CELEBRATE,// 勝利祝賀アニメーション中
	KNOCKBACK	// ノックバック（押し出し）アニメーション中
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




	void playerResourceLoader();

	int GetCurrentMovePoints() const { return m_currentMovePoints; }


	PlayerState GetState() const { return m_state; }

	int GetPreviewGridX() const { return m_previewGridX; }
	int GetPreviewGridZ() const { return m_previewGridZ; }

	// 攻撃距離を定数パラメータ化 (例: 1マス)
	const int ATTACK_RANGE = 1;
	// 勝利祝賀アニメーション関連
	void StartCelebration();// 勝利祝賀アニメーション開始
	bool IsCelebrationDone() const { return m_isCelebrationDone; }// 勝利祝賀アニメーションの完了フラグ
	virtual void OnPushed(Direction pushDir) override;// プレイヤーのノックバック（押し出し）処理

	//プレビューヒントのダメージ値を設定、必要に応じてオーバーライドして、予測ダメージの表示を制御
	virtual void SetPreviewDamage(int dmg) override;

private:
	//状態遷移関数
	void SwitchToMenuMain();
	void SwitchToMoveSelect();
	void SwitchToAttackDirSelect(AttackType type);
	void ExecuteMove();
	void ExecuteAttack();
	//入力処理関数
	void HandleMenuInput();
	void HandleMoveInput(float dt);
	void HandleAttackDirInput(float dt);
	//描画補助関数
	void DrawGhost();       // スタートポイントのプレーヤーのゴーストを描画
	void DrawPathLine();    // 移動予想ルートを描画

	virtual void StartTurn()override;
	virtual void EndTurn()override;
	virtual void TakeDamage(int damage, Unit* attacker)override;

	virtual void OnTurnChanged(TurnState state) override;
	Unit* GetTargetInLine(int range);

	bool UpdatePathMovement(float dt);

	//専用の移動予測計算関数を追加、現在の入力に基づいて移動予想位置を更新し、予想ダメージを計算する
	void CalculateMovePreviewDamage();


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
	int m_playerDamage = 1; // プレイヤーの攻撃力

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

	//プレーヤーカメラフェイス２用のフラグ
	bool m_isZoomedIn = false;

	bool m_isWaitingTurnStart = false; // ターン開始時のUIカットイン待ちフラグ
	bool m_canAttack = false;          // 攻撃可能な敵が範囲内にいるか
	// 勝利祝賀アニメーション用データ
	int m_jumpCount = 0;// ジャンプの回数
	float m_jumpTimer = 0.0f;// ジャンプの長さを制御するタイマー
	bool m_isCelebrationDone = false;// 祝賀アニメーションの完了フラグ
	void UpdateCelebration(float dt);// 勝利祝賀アニメーションの更新処理

	protected:
		virtual void OnDrawFloorUI(uint64_t delta) override;
		virtual void OnDrawTransparent(uint64_t delta) override;
		virtual void OnDrawOverlay(uint64_t delta) override;

		void DrawAttackWarningFloor();
		void DrawAttackWarningOverlay();
};