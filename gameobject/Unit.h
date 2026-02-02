#pragma once
#include <array>

#include"GameObject.h"
#include"../system/Direction.h"
#include "../enum class/TurnState.h"
#include "../ui/HPBar.h"

class MapManager;
class TurnManager;
class Tile;
class CStaticMeshRenderer;

class Unit:public GameObject
{
public:

	enum class Team
	{
		Player,
		Enemy,
		Ally
	};
	//抽象的なBaseClassとして、引数がないコンストラクタは禁止、実例化も禁止
	explicit Unit(GameContext* context);
	Unit(const Unit&) = delete;
	Unit& operator=(const Unit&) = delete;
	virtual ~Unit() = default;

	int GetHP() const { return m_currentHP; }
	int GetMaxHP() const { return m_maxHP; }
	int GetUnitGridX()const { return  m_gridX; };
	int GetUnitGridZ()const { return  m_gridZ; };
	void SetGridPosition(int x, int z) { m_gridX = x; m_gridZ = z; }

	virtual void TakeDamage(int damage, Unit* attacker);

	bool IsValidMoveTarget(int targetX, int targetZ);

	virtual void OnHpChanged() {};//UI更新用フック,UpdateHpBar
	virtual void ResetMovePoints() { m_currentMovePoints = m_maxMovePoints; };
	virtual Team GetTeam()const { return m_team; };

	// モデル設定インターフェース ---
	// 子クラスの Init 内で呼び出し、前後両面のレンダラーを登録する
	void SetModelRenderers(CStaticMeshRenderer* front, CStaticMeshRenderer* back);

	void SetFacingFromVector(const Vector3& dir);// ベクトルから向きを計算
	void SetFacing(Direction newDir);             // 向きを手動で直接設定


	void StartAttackAnimation(const Vector3& targetPos);
	//コールバック関数でダメージ数字を表示する
	bool UpdateAttackAnimation(float dt, std::function<void()> onImpact);

	//攻撃受けたあとのぶつかりアニメーション
	void StartBumpAnimation(const Vector3& targetPos);
	//攻撃受けたあとの位置変化アニメーション
	void StartSlideAnimation(const Vector3& targetPos);
	bool UpdateSlideAnimation(uint64_t dt);
	//HP Barの描画
	virtual void DrawUI();

	// --- 共通モデル描画メソッド ---
	// 子クラスの OnDraw 内で呼び出し、キャラ本体の描画を実行する
	void DrawModel();

	// --- 反転（フリップ）アニメーション制御 ---
	void UpdateFlipAnimation(float dt);

protected:
	virtual void OnTurnChanged(TurnState state);
	virtual void StartTurn();
	virtual void EndTurn();
protected:

	// フリップアニメーションの種類を定義
	enum class FlipStyle {
		None,   // アニメーションなし
		Simple, // 単純な反転 (0 -> 90 -> 0) : 主に北向き（背面）用
		Swing   // スイング反転 (0 -> 90 -> -90 -> 0) : 左右の鏡像反転用
	};
	FlipStyle m_currentFlipStyle = FlipStyle::Swing;

	// --- レンダリング関連 ---
	CStaticMeshRenderer* m_frontRenderer = nullptr; // 正面用モデル
	CStaticMeshRenderer* m_backRenderer = nullptr;  // 背面用モデル
	CStaticMeshRenderer* m_currRenderer = nullptr;  // 現在描画中のモデル

	int m_maxHP = 5;
	int m_currentHP = 5;

	int m_gridX;
	int m_gridZ;

	// 目標回転角度
	Vector3	m_targetRot = { 0.0f,0.0f,0.0f };

	//移動力
	int m_maxMovePoints;
	int m_currentMovePoints;
	//移動アニメション　データ
	Vector3 m_targetWorldPos;
	float m_moveSpeed;
	//0: +Z, 1: +X, 2: -Z, 3: -X
	Direction m_facing;
	Team m_team;

	//アニメの変数
	Vector3 m_animStartPos;
	Vector3 m_animLungePos;
	float m_animTimer = 0.0f;
	bool m_animHasHit = false;//ダメージを一回だけの検査
	const float TIME_LUNGE = 0.1f;
	const float TIME_IMPACT = 0.15f;
	const float TIME_RETURN = 0.20f;
	const float TIME_END = 0.3f;
	//変数：攻撃受けたあとの位置変化アニメーション
	Vector3 m_slideStartPos;
	Vector3 m_slideEndPos;
	float m_slideTimer = 0.0f;
	
	// --- 左右の向きの記憶用フラグ ---
	// true = 右向き（東/南）、false = 左向き（西）
	// 背面（北）を向いた際にも、直前の左右の視覚的な傾向を維持するために使用する
	bool m_isFacingRight = true;

	std::unique_ptr<HPBar> m_hpBar;


	// --- 反転アニメーションの状態管理 ---
	bool m_isFlipping = false;          // 反転動作中フラグ
	float m_flipTimer = 0.0f;           // 反転用タイマー
	const float FLIP_DURATION = 0.4f;   // 反転にかかる合計時間 (0.2秒)
	Direction m_nextFacing = Direction::South; // 遷移先の目標方向
	bool m_hasSwappedMesh = false;      // メッシュの切り替えが完了したかどうかの判定フラグ
};