#pragma once

#include <array>
#include <functional>
#include <memory>

#include "GameObject.h"
#include "../../Types/Direction.h"
#include "../../Types/TurnState.h"
#include "../../UI/Component/HPBar.h"
#include "../../System/MeshManager.h"
#include "../../GamePlay/Manager/TurnManager.h"

class MapManager;
struct Tile;
class CStaticMeshRenderer;
class CShader;

// =========================================================
// Unit クラス
// プレイヤー、敵、味方など、盤面上を移動し戦闘を行うエンティティの共通基底クラス
// HP管理、グリッド座標、ダメージ計算、共有アニメーション（ノックバック等）を担う
// =========================================================
class Unit : public GameObject {
public:

	explicit Unit(GameContext* context);
	Unit(const Unit&) = delete;
	Unit& operator=(const Unit&) = delete;
	virtual ~Unit();

	// ---------------------------------------------------------
	// ライフサイクル (Lifecycle)
	// ---------------------------------------------------------
	virtual void Update(float deltaSeconds) override;

	// ---------------------------------------------------------
	// コアステータス・ゲッター (Core Status)
	// ---------------------------------------------------------
	int GetHP() const { return m_currentHP; }
	int GetMaxHP() const { return m_maxHP; }
	int GetUnitGridX() const { return m_gridX; }
	int GetUnitGridZ() const { return m_gridZ; }
	Direction GetFacing() const { return m_facing; }

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

	// (fromX, fromZ) から pushDir 方向へ押し出された場合の連鎖ダメージ（衝突 or 罠）を予測。
	// 実結算と移動プレビューが同一ルールを共有するための static 純関数。
	// ignoreOccupant: 占有判定から除外するユニット（プレビュー時、移動後に空く自分自身のマス対策）
	static int SimulatePushChainDamage(MapManager* map, int fromX, int fromZ,
		Direction pushDir, int collisionDamage,
		const Unit* ignoreOccupant = nullptr);
	// 攻撃プレビューヒントのダメージ値を設定（UI描画用）
	virtual void SetPreviewDamage(int dmg) { m_previewDamage = dmg; }
	void DebugSetHP(int hp) { m_currentHP = hp; }
	bool IsValidMoveTarget(int targetX, int targetZ);
	// 無敵状態のゲッター・セッター
	bool IsInvincible() const { return m_isInvincible; }
	void SetInvincible(bool value) { m_isInvincible = value; }

	bool CanTarget(const Unit* other) const {
		if (other == nullptr) return false;
		if (other == this) return false;
		if (other->IsDead()) return false;
		if (other->IsInvincible()) return false; 
		return true;
	}

	// ---------------------------------------------------------
	// アニメーション制御 (Animation System)
	// ---------------------------------------------------------
	void SetFacingFromVector(const Vector3& dir);
	void SetFacing(Direction newDir);

	void StartAttackAnimation(const Vector3& targetPos);
	bool UpdateAttackAnimation(float dt, std::function<void()> onImpact);

	void StartBumpAnimation(const Vector3& targetPos);
	void StartSlideAnimation(const Vector3& targetPos);
	bool UpdateSlideAnimation(float deltaSeconds);

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

	// --- アウトライン上書き（保護対象の強調表示用） ---
	void SetOutlineOverride(const Color& color) {
		m_hasOutlineOverride = true;
		m_outlineOverrideColor = color;
	}
	void ClearOutlineOverride() { m_hasOutlineOverride = false; }

	// ディザによるフェード：目標値を外部から設定し、m_fade は毎フレーム滑らかに遷移
	void  SetTargetFade(float f) { m_targetFade = f; }
	void  SetFade(float f) { m_fade = f; m_targetFade = f; } // 即時設定（既存インターフェースを維持）
	float GetFade() const { return m_fade; }
public:
	static inline bool s_hpBarVisible = true;  // HPバー表示切替（DebugUIから制御）
	// 死亡飛翔中、十字スター出現によりモデルが非表示になったか（KillCam の定格判定用）
	bool IsDeathVisualHidden() const { return m_isDeathVisualHidden; }

protected:
	virtual void OnTurnChanged(TurnState state);
	virtual void StartTurn();
	virtual void EndTurn();
	void DrawOutline();
	void DrawBlobShadow();

	class MapManager* GetMap() const {
		return m_context ? m_context->GetMapManager() : nullptr;
	}

	class TurnManager* GetTurnManager() const {
		return m_context ? m_context->GetTurnManager() : nullptr;
	}

	class EffectManager* GetEffectManager() const {
		return m_context ? m_context->GetEffectManager() : nullptr;
	}

	// fade 値を本体のすべての子マテリアルの Dummy.x に設定
	// ToonPS / OutlinePS のディザリングクリップに使用
	void ApplyFadeToMaterials(float fade);

protected:
	// =========================================================
	// メンバー変数 (Member Variables)
	// =========================================================

	// --- レンダリング関連 ---
	CStaticMeshRenderer* m_renderer = nullptr;
	// --- 描画リソースキャッシュ（毎フレームの文字列検索を回避） ---
	// シェーダーはシーンInitで登録済み、ユニットはレベルロードで再生成されるため
	// キャッシュの生存期間はリソースと一致する（初回使用時に取得）
	CShader* m_toonShader = nullptr;
	CShader* m_outlineShader = nullptr;
	CShader* m_blobShader = nullptr;
	CStaticMeshRenderer* m_blobMesh = nullptr;
	CStaticMeshRenderer* m_pushArrowMesh = nullptr;
	float m_targetFade = 0.0f;

	std::unique_ptr<HPBar> m_hpBar;
	// --- アウトライン上書き ---
    bool  m_hasOutlineOverride = false;
    Color m_outlineOverrideColor = Color(0, 0, 0, 0);  // .w = アウトライン幅

	// --- コアステータス ---
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

	bool m_isInvincible = false;

	// --- アニメーション制御データ ---
	Vector3 m_animStartPos;
	Vector3 m_animLungePos;
	float m_animTimer = 0.0f;
	bool m_hasAnimHit = false; // 攻撃の多段ヒット防止フラグ

	Vector3 m_slideStartPos;
	Vector3 m_slideEndPos;
	float m_slideTimer = 0.0f;
	bool  m_slideTumbleOnX = true;  // ノックバックされた時の翻転軸（X軸かZ軸か）を決定するフラグ
	float m_slideTumbleSign = 1.0f; // ノックバック時の翻転方向を決定する符号（+1 or -1）

	bool m_isTurning = false;

	// --- 死亡飛出 ---
	Vector3 m_hitSourcePos = {};
	Vector3 m_deathVelocity = {};
	Vector3 m_deathSpin = {};
	bool m_isDeathFlying = false;
	// --- 死亡飛翔の演出（トレイル＋消滅スター） ---
	float m_deathFlyTimer = 0.0f;
	float m_deathArcTime = 0.0f;        // 対称放物線の周期（進行率の分母）
	float m_deathTrailTimer = 0.0f;
	bool  m_deathStarSpawned = false;
	bool  m_isDeathVisualHidden = false; // スター出現後モデル非表示


	// TurnManager への購読ID（デストラクタで自動解除）
	TurnManager::ScopedConnection m_turnConnection;
	//ディザリングクリップに使用
	float m_fade = 0.0f;
};