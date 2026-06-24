#pragma once

#include <array>
#include <functional>
#include <memory>

#include "GameObject.h"
#include "../../System/Direction.h"
#include "../../EnumClass/TurnState.h"
#include "../../UI/Component/HPBar.h"
#include "../../System/meshmanager.h"

class MapManager;
class TurnManager;
class Tile;
class CStaticMeshRenderer;

// =========================================================
// Unit クラス
// プレイヤー、敵、味方など、盤面上を移動し戦闘を行うエンティティの共通基底クラス
// HP管理、グリッド座標、ダメージ計算、共有アニメーション（ノックバック等）を担う
// =========================================================
class Unit : public GameObject {
public:
	enum class Team {
		Player,
		Enemy,
		Ally
	};

	explicit Unit(GameContext* context);
	Unit(const Unit&) = delete;
	Unit& operator=(const Unit&) = delete;
	virtual ~Unit() = default;

	// ---------------------------------------------------------
	// ライフサイクル (Lifecycle)
	// ---------------------------------------------------------
	virtual void Update(uint64_t delta) override;

	// ---------------------------------------------------------
	// コアステータス・ゲッター (Core Status)
	// ---------------------------------------------------------
	int GetHP() const { return m_currentHP; }
	int GetMaxHP() const { return m_maxHP; }
	int GetUnitGridX() const { return m_gridX; }
	int GetUnitGridZ() const { return m_gridZ; }
	Direction GetFacing() const { return m_facing; }
	virtual Team GetTeam() const { return m_team; }

	void SetGridPosition(int x, int z) { m_gridX = x; m_gridZ = z; }
	virtual void ResetMovePoints() { m_currentMovePoints = m_maxMovePoints; }

	// ---------------------------------------------------------
	// 戦闘と物理干渉 (Combat & Physics)
	// ---------------------------------------------------------
	virtual void TakeDamage(int damage, Unit* attacker);

	// 押し出し（ノックバック）を受けた際の処理。壁衝突時は false を返す想定など、派生クラスで拡張可能
	virtual void OnPushed(Direction pushDir, Unit* attacker = nullptr);

	// 押し出しを伴う攻撃を受けた際、壁や罠による二次ダメージを含めた最終被ダメージを算出
	int CalculateExpectedDamage(int baseDamage, bool isPush, Direction pushDir);

	// 攻撃プレビューヒントのダメージ値を設定（UI描画用）
	virtual void SetPreviewDamage(int dmg) { m_previewDamage = dmg; }
	void DebugSetHP(int hp) { m_currentHP = hp; }
	bool IsValidMoveTarget(int targetX, int targetZ);

	// ---------------------------------------------------------
	// アニメーション制御 (Animation System)
	// ---------------------------------------------------------
	void SetFacingFromVector(const Vector3& dir);
	void SetFacing(Direction newDir);

	void StartAttackAnimation(const Vector3& targetPos);
	bool UpdateAttackAnimation(float dt, std::function<void()> onImpact);

	void StartBumpAnimation(const Vector3& targetPos);
	void StartSlideAnimation(const Vector3& targetPos);
	bool UpdateSlideAnimation(uint64_t dt);

	void UpdateFacingRotation(float dt);

	// ---------------------------------------------------------
	// 死亡飛出演出 (Death Fly)
	// ---------------------------------------------------------
	void StartDeathFly();
	void UpdateDeathFly(float delta);
	virtual void OnDeathFlyComplete();

	// ---------------------------------------------------------
	// レンダリングとUI (Rendering & UI)
	// ---------------------------------------------------------
	void SetModelRenderer(CStaticMeshRenderer* r);
	void DrawModel();
	virtual void DrawUI();
	void DrawPushPreview(Direction pushDir);

	virtual void OnHpChanged() {};

protected:
	virtual void OnTurnChanged(TurnState state);
	virtual void StartTurn();
	virtual void EndTurn();

protected:
	// =========================================================
	// メンバー変数 (Member Variables)
	// =========================================================

	// --- レンダリング関連 ---
	CStaticMeshRenderer* m_Renderer = nullptr;

	std::unique_ptr<HPBar> m_hpBar;

	// --- コアステータス ---
	Team m_team;
	int m_maxHP = 5;
	int m_currentHP = 5;
	int m_gridX = 0;
	int m_gridZ = 0;
	int m_maxMovePoints = 0;
	int m_currentMovePoints = 0;
	float m_moveSpeed = 0.0f;
	Direction m_facing;

	int m_previewDamage = 0;
	int m_onPushDamage = 2;  // ノックバック壁衝突時の基本ダメージ量

	Vector3 m_targetRot = { 0.0f, 0.0f, 0.0f };
	Vector3 m_targetWorldPos;

	// --- アニメーション制御データ ---
	Vector3 m_animStartPos;
	Vector3 m_animLungePos;
	float m_animTimer = 0.0f;
	bool m_hasAnimHit = false; // 攻撃の多段ヒット防止フラグ

	Vector3 m_slideStartPos;
	Vector3 m_slideEndPos;
	float m_slideTimer = 0.0f;

	bool m_isTurning = false;

	// --- 死亡飛出 ---
	Vector3 m_hitSourcePos = {};
	Vector3 m_deathVelocity = {};
	Vector3 m_deathSpin = {};

};