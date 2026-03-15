#include "Unit.h"
#include "../manager/GameContext.h"
#include "../manager/MapManager.h"
#include "../manager/TurnManager.h"
#include "../ui/DamageNumberManager.h"
#include "../manager/EffectManager.h"
#include "../gameobject/Trap.h"
#include <cmath>

namespace {
	// ---------------------------------------------------------
	// 演出・バランス用定数（マジックナンバーの排除）
	// ---------------------------------------------------------
	const float HIT_EFFECT_Y_OFFSET = 0.8f;      // ヒットエフェクトの発生高さ（胸〜頭付近）
	const float DAMAGE_NUM_Y_OFFSET = 1.0f;      // ダメージ数字の発生高さ
	const float HIT_POS_RANDOM_SPREAD = 0.3f;    // ヒットエフェクトの散らばり幅
	const float DAMAGE_NUM_RANDOM_SPREAD = 0.5f; // ダメージ数字の散らばり幅

	const float LUNGE_DISTANCE = 0.7f;           // 攻撃時の踏み込み距離
	const float TIME_LUNGE = 0.1f;               // 攻撃：踏み込みにかかる時間
	const float TIME_IMPACT = 0.15f;             // 攻撃：ダメージ判定が発生する瞬間
	const float TIME_RETURN = 0.20f;             // 攻撃：戻り始める時間
	const float TIME_END = 0.3f;                 // 攻撃：元の位置に戻る（完了）時間

	const float TIME_SLIDE = 0.2f;               // ノックバック（スライディング）の所要時間
	const float FLIP_DURATION = 0.4f;            // 振り向き（反転）アニメーションの所要時間

	const float ARROW_Y_OFFSET = 0.08f;          // プレビュー矢印のZファイティング防止用浮かし幅
}

Unit::Unit(GameContext* context) : GameObject(context) {
	if (m_context && m_context->GetTurnManager()) {
		m_context->GetTurnManager()->RegisterObserver(
			[this](TurnState state) { this->OnTurnChanged(state); }
		);
	}
	else {
		std::cerr << "[Error] Unit created but TurnManager is NULL! Observer failed." << std::endl;
	}

	m_hpBar = std::make_unique<HPBar>();
	m_hpBar->Init(context);
}

// ---------------------------------------------------------

// ライフサイクル (Lifecycle)

// ---------------------------------------------------------

void Unit::Update(uint64_t delta) {
	float dt = static_cast<float>(delta) / 1000.0f;
	if (m_hpBar) {
		m_hpBar->Update(dt);
	}
}

void Unit::StartTurn() {}
void Unit::EndTurn() {}
void Unit::OnTurnChanged(TurnState state) {}

// ---------------------------------------------------------

// 戦闘と物理干渉 (Combat & Physics)

// ---------------------------------------------------------

void Unit::TakeDamage(int damage, Unit* attacker) {
	if (damage < 0) return;
	m_currentHP = std::max(0, m_currentHP - damage);

	// 視覚的重複の回避：複数回ダメージを受けた際、エフェクトや数字が
	
	// 完全に重なって見えなくなるのを防ぐため、ランダムなオフセットを加える
	if (m_context && m_context->GetEffectManager()) {
		Vector3 hitPos = m_srt.pos;
		hitPos.y += HIT_EFFECT_Y_OFFSET;
		hitPos.x += ((rand() % 10) / 10.0f - 0.5f) * HIT_POS_RANDOM_SPREAD;
		hitPos.z += ((rand() % 10) / 10.0f - 0.5f) * HIT_POS_RANDOM_SPREAD;

		m_context->GetEffectManager()->SpawnHitEffect(hitPos);
	}

	if (m_context && m_context->GetDamageManager()) {
		Vector3 headPos = m_srt.pos;
		headPos.y += DAMAGE_NUM_Y_OFFSET;
		headPos.x += ((rand() % 10) / 10.0f - 0.5f) * DAMAGE_NUM_RANDOM_SPREAD;

		m_context->GetDamageManager()->SpawnDamage(headPos, damage);
	}

	OnHpChanged();
}

int Unit::CalculateExpectedDamage(int baseDamage, bool isPush, Direction pushDir) {
	int expectedDamage = baseDamage;

	// 押し出し攻撃の場合、移動先の状況によって追加の連鎖ダメージを予測する
	if (isPush && m_context && m_context->GetMapManager()) {
		DirOffset offset = DirOffset::From(pushDir);
		int pushX = m_gridX + offset.x;
		int pushZ = m_gridZ + offset.z;

		bool isBlocked = !m_context->GetMapManager()->IsWalkable(pushX, pushZ);
		Tile* nextTile = m_context->GetMapManager()->GetTile(pushX, pushZ);
		if (nextTile && nextTile->occupant) isBlocked = true;

		if (isBlocked) {
			// 壁や他のユニットに激突する場合
			expectedDamage += m_onPushDamage;
		}
		else if (nextTile && nextTile->structure) {
			// 衝突せずスムーズに押し出される場合、足元のギミック（罠）を確認
			if (nextTile->structure->GetType() == MapModelType::TRAP) {
				Trap* trap = dynamic_cast<Trap*>(nextTile->structure);
				// 未発動の罠であれば、トラップダメージが加算されると予測
				if (trap && !trap->IsActivated()) {
					expectedDamage += trap->GetTrapDamage();
				}
			}
		}
	}
	return expectedDamage;
}

bool Unit::IsValidMoveTarget(int targetX, int targetZ) {
	if (m_context->GetMapManager() == nullptr) return false;
	return m_context->GetMapManager()->IsWalkable(targetX, targetZ);
}

void Unit::OnPushed(Direction pushDir) {
	if (m_currentHP <= 0) return;

	DirOffset offset = DirOffset::From(pushDir);
	int targetX = m_gridX + offset.x;
	int targetZ = m_gridZ + offset.z;

	MapManager* map = m_context->GetMapManager();
	bool isBlocked = !(map->IsWalkable(targetX, targetZ));
	Tile* targetTile = map->GetTile(targetX, targetZ);

	Unit* obstacleUnit = nullptr;
	if (targetTile && targetTile->occupant) {
		isBlocked = true;
		obstacleUnit = targetTile->occupant;
	}

	if (isBlocked) {
		// 障害物に衝突した場合の処理
		Vector3 obstaclePos = map->GetWorldPosition(targetX, targetZ);
		StartBumpAnimation(obstaclePos);

		TakeDamage(m_onPushDamage, nullptr);
		if (obstacleUnit) {
			// 連鎖衝突：ぶつかられたユニットもダメージを受ける
			obstacleUnit->TakeDamage(m_onPushDamage, this);
		}
	}
	else {
		// 障害物がない場合の移動処理
		Tile* currentTile = map->GetTile(m_gridX, m_gridZ);
		if (currentTile) currentTile->occupant = nullptr;

		m_gridX = targetX;
		m_gridZ = targetZ;
		if (targetTile) targetTile->occupant = this;

		Vector3 targetWorldPos = map->GetWorldPosition(targetX, targetZ);
		StartSlideAnimation(targetWorldPos);
	}
}

// ---------------------------------------------------------

// アニメーション制御 (Animation System)

// ---------------------------------------------------------

void Unit::SetFacingFromVector(const Vector3& dir) {
	// 微小なベクトルによる不要な振り向き（フリッカー）を防止
	if (dir.LengthSquared() < 0.0001f) return;
	Direction newDir = DirOffset::FromVector(dir.x, dir.z);
	SetFacing(newDir);
}

void Unit::SetFacing(Direction newDir) {
	if (m_facing == newDir || m_isFlipping) return;

	// 南(正面) と 東(正面) は同じメッシュ・スケールを使用するため、アニメーションを省略して即時反映
	bool isCurrentNormal = (m_facing == Direction::South || m_facing == Direction::East);
	bool isNextNormal = (newDir == Direction::South || newDir == Direction::East);

	if (isCurrentNormal && isNextNormal) {
		m_facing = newDir;
		return;
	}

	if (m_facing == Direction::North || newDir == Direction::North) {
		m_currentFlipStyle = FlipStyle::Simple;
	}
	else {
		m_currentFlipStyle = FlipStyle::Swing;
	}

	m_nextFacing = newDir;
	m_isFlipping = true;
	m_flipTimer = 0.0f;
	m_hasSwappedMesh = false;
}

void Unit::StartAttackAnimation(const Vector3& targetPos) {
	m_animStartPos = m_srt.pos;
	m_animTimer = 0.0f;
	m_hasAnimHit = false;

	Vector3 dir = targetPos - m_animStartPos;
	if (dir.LengthSquared() > 0.001f) dir.Normalize();

	m_animLungePos = m_animStartPos + dir * LUNGE_DISTANCE;
	SetFacingFromVector(dir);
}

bool Unit::UpdateAttackAnimation(float dt, std::function<void()> onImpact) {
	float deltaSeconds = dt / 1000.0f;
	m_animTimer += deltaSeconds;

	// 設定したインパクト時間に達した場合、ダメージ判定のコールバックを発火させる
	if (!m_hasAnimHit && m_animTimer >= TIME_IMPACT) {
		m_hasAnimHit = true;
		if (onImpact) onImpact();
	}

	bool isFinished = false;

	// フェーズ1：対象に向かって素早く踏み込む（Lunge）
	if (m_animTimer < TIME_LUNGE) {
		float t = m_animTimer / TIME_LUNGE;
		m_srt.pos = Vector3::Lerp(m_animStartPos, m_animLungePos, t);
	}
	// フェーズ2：衝突した瞬間の停止感（ヒットストップ）を演出
	else if (m_animTimer < TIME_RETURN) {
		m_srt.pos = m_animLungePos;
	}
	// フェーズ3：元の位置にスムーズに戻る（Return）
	else if (m_animTimer < TIME_END) {
		float t = (m_animTimer - TIME_RETURN) / (TIME_END - TIME_RETURN);
		m_srt.pos = Vector3::Lerp(m_animLungePos, m_animStartPos, t);
	}
	else {
		m_srt.pos = m_animStartPos;
		isFinished = true;
	}

	UpdateWorldMatrix();
	Renderer::SetWorldMatrix(&m_WorldMatrix);

	return isFinished;
}

void Unit::StartBumpAnimation(const Vector3& targetPos) {
	m_animStartPos = m_srt.pos;
	m_animTimer = 0.0f;
	m_slideTimer = 0.0f;
	m_hasAnimHit = false;

	Vector3 dir = targetPos - m_animStartPos;
	if (dir.LengthSquared() > 0.001f) dir.Normalize();
	m_animLungePos = m_animStartPos + dir * LUNGE_DISTANCE;
}

void Unit::StartSlideAnimation(const Vector3& targetPos) {
	m_slideStartPos = m_srt.pos;
	m_slideEndPos = targetPos;
	m_animTimer = 0.0f;
	m_slideTimer = 0.0f;
}

bool Unit::UpdateSlideAnimation(uint64_t dt) {
	float deltaSeconds = dt / 1000.0f;
	m_slideTimer += deltaSeconds;

	if (m_slideTimer < TIME_SLIDE) {
		float t = m_slideTimer / TIME_SLIDE;
		// EaseOutQuadの適用:「最初は速く、停止直前はゆっくり」という自然な減速感を作る
		t = 1.0f - std::pow(1.0f - t, 2.0f);

		m_srt.pos = Vector3::Lerp(m_slideStartPos, m_slideEndPos, t);

		UpdateWorldMatrix();
		Renderer::SetWorldMatrix(&m_WorldMatrix);
		return false;
	}
	else {
		m_srt.pos = m_slideEndPos;
		UpdateWorldMatrix();
		Renderer::SetWorldMatrix(&m_WorldMatrix);
		return true;
	}
}

void Unit::UpdateFlipAnimation(float dt) {
	if (!m_isFlipping) {
		m_srt.rot.y = 0.0f;
		return;
	}

	m_flipTimer += dt;
	float t = m_flipTimer / FLIP_DURATION;
	float visualRotY = 0.0f;

	// フェーズ1: 0度 -> 90度 (紙が裏返るように細くなる)
	if (t < 0.5f) {
		float phaseT = t / 0.5f;
		visualRotY = std::lerp(0.0f, PI / 2.0f, phaseT);
	}
	// フェーズ2: 90度 -> 0度 (メッシュを切り替えて厚みが戻る)
	else {
		if (!m_hasSwappedMesh) {
			m_facing = m_nextFacing;
			m_hasSwappedMesh = true;

			// 左右の視覚的な向きを記録
			if (m_facing == Direction::East || m_facing == Direction::South) {
				m_isFacingRight = true;
			}
			else if (m_facing == Direction::West) {
				m_isFacingRight = false;
			}

			float targetScaleX = 1.0f;

			if (m_facing == Direction::North) {
				m_currRenderer = m_backRenderer;
				// 背面モデルの標準が「左向き」であると仮定、右向きを維持する場合は反転させる
				targetScaleX = m_isFacingRight ? -1.0f : 1.0f;
			}
			else {
				m_currRenderer = m_frontRenderer;
				// 正面モデルの標準が「右向き」であると仮定、左向きにする場合は反転させる
				targetScaleX = m_isFacingRight ? 1.0f : -1.0f;
			}

			m_srt.scale.x = targetScaleX;
		}

		float phaseT = (t - 0.5f) / 0.5f;
		visualRotY = std::lerp(PI / 2.0f, 0.0f, phaseT);
	}

	if (t >= 1.0f) {
		m_isFlipping = false;
		visualRotY = 0.0f;
	}

	m_srt.rot.y = visualRotY;
}

// ---------------------------------------------------------

// レンダリングとUI (Rendering & UI)

// ---------------------------------------------------------

void Unit::SetModelRenderers(CStaticMeshRenderer* front, CStaticMeshRenderer* back) {
	m_frontRenderer = front;
	m_backRenderer = back;

	// Unlitシェーダー適用時、モデルのベースカラーが暗くならないようマテリアルを純白に強制する
	auto ForceWhiteMaterial = [](CStaticMeshRenderer* renderer) {
		if (!renderer) return;

		if (auto* mat = renderer->GetMaterial(0)) {
			MATERIAL m = mat->GetData();
			m.Diffuse = Color(1.0f, 1.0f, 1.0f, 1.0f);
			m.Ambient = Color(1.0f, 1.0f, 1.0f, 1.0f);
			m.Emission = Color(0.1f, 0.1f, 0.1f, 1.0f);
			m.TextureEnable = TRUE;
			mat->SetMaterial(m);
		}
		};

	ForceWhiteMaterial(m_frontRenderer);
	ForceWhiteMaterial(m_backRenderer);

	m_currRenderer = m_frontRenderer;
	m_srt.scale = Vector3(1.0f, 1.0f, 1.0f);
	m_srt.rot = Vector3(0.0f, 0.0f, 0.0f);
}

void Unit::DrawModel() {
	if (!m_currRenderer) return;
	Renderer::SetWorldMatrix(&m_WorldMatrix);
	m_currRenderer->Draw();
}

void Unit::DrawUI() {
	if (!m_hpBar || m_currentHP <= 0) return;

	m_hpBar->Draw(m_srt.pos, m_currentHP, m_maxHP, m_previewDamage);

	// 描画後にプレビューダメージを即座にリセットすることで、
	
	// ターゲットされている（エイムされている）フレームのみUIが点滅するように制御する
	m_previewDamage = 0;
}

void Unit::DrawPushPreview(Direction pushDir) {
	auto* pushArrowRenderer = MeshManager::getRenderer<CStaticMeshRenderer>("arrow_push_mesh");
	if (!pushArrowRenderer || !m_context || !m_context->GetMapManager()) return;

	MapManager* map = m_context->GetMapManager();
	DirOffset offset = DirOffset::From(pushDir);

	int targetX = m_gridX + offset.x;
	int targetZ = m_gridZ + offset.z;

	bool isBlocked = !map->IsWalkable(targetX, targetZ);
	Tile* targetTile = map->GetTile(targetX, targetZ);
	if (targetTile && targetTile->occupant) isBlocked = true;

	Vector3 myPos = map->GetWorldPosition(m_gridX, m_gridZ);
	Vector3 targetPos = map->GetWorldPosition(targetX, targetZ);

	// 描画位置を現在地と対象地の間に設定
	Vector3 arrowPos = myPos + (targetPos - myPos) * 0.2f;
	arrowPos.y += ARROW_Y_OFFSET;

	float rotY = 0.0f;
	if (offset.x == 1)       rotY = 0.0f;
	else if (offset.x == -1) rotY = PI;
	else if (offset.z == 1)  rotY = -PI / 2.0f;
	else if (offset.z == -1) rotY = PI / 2.0f;

	Matrix4x4 world = Matrix4x4::CreateScale(Vector3(1.0f, 1.0f, 1.5f))
		* Matrix4x4::CreateRotationY(rotY)
		* Matrix4x4::CreateTranslation(arrowPos);

	// 衝突が予測される場合は黄色、安全に移動できる場合は灰色で視覚的フィードバックを提供する
	Color arrowColor = isBlocked ? Color(1.0f, 1.0f, 0.0f, 0.7f) : Color(0.6f, 0.6f, 0.6f, 0.9f);

	Renderer::SetWorldMatrix(&world);
	if (auto* mat = pushArrowRenderer->GetMaterial(0)) {
		MATERIAL old = mat->GetData();
		MATERIAL temp = old;
		temp.Diffuse = arrowColor;
		mat->SetMaterial(temp);
		pushArrowRenderer->Draw();
		mat->SetMaterial(old);
	}

	if (isBlocked && m_context->GetEffectManager()) {
		Vector3 effectPos = targetPos;
		effectPos.y += HIT_EFFECT_Y_OFFSET;
		m_context->GetEffectManager()->DrawStaticHitPreview(effectPos);
	}
}