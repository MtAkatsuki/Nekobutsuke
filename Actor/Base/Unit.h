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
	virtual void OnPushed(Direction pushDir);

	// 押し出しを伴う攻撃を受けた際、壁や罠による二次ダメージを含めた最終被ダメージを算出
	int CalculateExpectedDamage(int baseDamage, bool isPush, Direction pushDir);

	// 攻撃プレビューヒントのダメージ値を設定（UI描画用）
	virtual void SetPreviewDamage(int dmg) { m_previewDamage = dmg; }

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

	void UpdateFlipAnimation(float dt);

	// ---------------------------------------------------------
	// レンダリングとUI (Rendering & UI)
	// ---------------------------------------------------------
	void SetModelRenderers(CStaticMeshRenderer* front, CStaticMeshRenderer* back);
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

	// フリップアニメーションの挙動スタイル
	enum class FlipStyle {
		None,
		Simple, // 0 -> 90 -> 0 (北向き背面用)
		Swing   // 0 -> 90 -> -90 -> 0 (左右鏡像反転用)
	};
	FlipStyle m_currentFlipStyle = FlipStyle::Swing;

	// --- レンダリング関連 ---
	CStaticMeshRenderer* m_frontRenderer = nullptr;
	CStaticMeshRenderer* m_backRenderer = nullptr;
	CStaticMeshRenderer* m_currRenderer = nullptr;

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

	bool m_isFlipping = false;
	float m_flipTimer = 0.0f;
	Direction m_nextFacing = Direction::South;
	bool m_hasSwappedMesh = false;

	// 左右の向きの記憶（true = 東/南の右向き、false = 西の左向き）
	// 北(背面)を向いた際も、直前の左右の視覚的傾向を維持するために使用する
	bool m_isFacingRight = true;
};