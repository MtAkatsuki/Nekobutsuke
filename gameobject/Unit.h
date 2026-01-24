#pragma once
#include <array>

#include"GameObject.h"
#include"../system/Direction.h"
#include "../enum class/TurnState.h"
#include "../ui/HPBar.h"

class MapManager;
class TurnManager;
class Tile;

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

	void SetFacingFromVector(const Vector3& dir);
	void StartAttackAnimation(const Vector3& targetPos);
	//コールバック関数でダメージ数字を表示する
	bool UpdateAttackAnimation(float dt, std::function<void()> onImpact);

	//攻撃受けたあとのぶつかりアニメーション
	void StartBumpAnimation(const Vector3& targetPos);
	//攻撃受けたあとの位置変化アニメーション
	void StartSlideAnimation(const Vector3& targetPos);
	bool UpdateSlideAnimation(uint64_t dt);
	//HP Barの描画
	void DrawUI();

protected:
	virtual void OnTurnChanged(TurnState state);
	virtual void StartTurn();
	virtual void EndTurn();

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
	
	std::unique_ptr<HPBar> m_hpBar;
};