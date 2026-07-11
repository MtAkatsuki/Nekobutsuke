#include "Unit.h"
#include "../../Core/GameContext.h"
#include "../../GamePlay/Manager/MapManager.h"
#include "../../GamePlay/Manager/TurnManager.h"
#include "../../UI/System/DamageNumberManager.h"
#include "../../GamePlay/Manager/EffectManager.h"
#include "../../Actor/Gimmick/Trap.h"
#include "../../System/ZFightTunables.h"
#include <cmath>
#include "../../System/RandomEngine.h"

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
	const float FACING_LERP_SPEED = 14.0f;		// 12〜18 で手触り調整
	const float MODEL_FORWARD_OFFSET = 0.0f;	// ← モデル導入後の一回限り標定

	const float BLOB_SIZE = 1.2f;                // Blob影のスケール（半径）

	// 死亡飛出
	const float DEATH_GRAVITY = 50.0f;
	const float DEATH_FLY_FORCE = 30.0f;
	const float DEATH_FLY_DIR_XZ = 1.8f;     // 飛出方向の水平成分の倍率
	const float DEATH_FLY_DIR_Y = 0.6f;      // 飛出方向の上向き成分
	const float DEATH_SPIN_MAX = 10.0f;      // 飛出時のランダム回転速度の上限（rad/s）
	const float DEATH_FALL_KILL_Y = -20.0f;  // この高さまで落下したら消滅処理を行う

	// ノックバック（スライディング）演出パラメータ
	const float SLIDE_ARC_HEIGHT = 1.0f;   // ノックバック曲線の頂点高さ（落下時の弧の高さ）
	const float SLIDE_TUMBLE_TURNS = 0.0f;  // 転がる回転の回数（1.0 = 360度1回転）

	// 押し出しプレビュー矢印の表示パラメータ
	const float PUSH_ARROW_POS_RATIO = 0.2f;                            // 現在地→対象地間の矢印配置比率
	const Vector3 PUSH_ARROW_SCALE = Vector3(1.0f, 1.0f, 1.5f);         // 矢印の表示スケール（進行方向に伸ばす）
	const Color PUSH_ARROW_WARN_COLOR = Color(1.0f, 1.0f, 0.0f, 0.7f);  // 衝突予測時：黄色
	const Color PUSH_ARROW_SAFE_COLOR = Color(0.6f, 0.6f, 0.6f, 0.9f);  // 安全移動時：灰色
}

Unit::Unit(GameContext* context) : GameObject(context) {
	if (m_context && m_context->GetTurnManager()) {
		m_context->GetTurnManager()->RegisterObserver(
			[this](TurnState state) { this->OnTurnChanged(state); }
		);
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
	if (attacker) m_hitSourcePos = attacker->GetSRT().pos;

	// 視覚的重複の回避：複数回ダメージを受けた際、エフェクトや数字が
	
	// 完全に重なって見えなくなるのを防ぐため、ランダムなオフセットを加える
	if (m_context && m_context->GetEffectManager()) {
		auto& rng = RandomEngine::tls();
		Vector3 hitPos = m_srt.pos;
		hitPos.y += HIT_EFFECT_Y_OFFSET;
		hitPos.x += static_cast<float>(rng.uniformReal(-0.5, 0.5)) * HIT_POS_RANDOM_SPREAD;
		hitPos.z += static_cast<float>(rng.uniformReal(-0.5, 0.5)) * HIT_POS_RANDOM_SPREAD;

		m_context->GetEffectManager()->SpawnHitEffect(hitPos);
	}

	if (m_context && m_context->GetDamageManager()) {
		Vector3 headPos = m_srt.pos;
		headPos.y += DAMAGE_NUM_Y_OFFSET;
		headPos.x += static_cast<float>(RandomEngine::tls().uniformReal(-0.5, 0.5)) * DAMAGE_NUM_RANDOM_SPREAD;

		m_context->GetDamageManager()->SpawnDamage(headPos, damage);
	}

	OnHpChanged();
}

int Unit::SimulatePushChainDamage(MapManager* map, int fromX, int fromZ,
	Direction pushDir, int collisionDamage,
	const Unit* ignoreOccupant) {
	if (!map) return 0;

	DirOffset offset = DirOffset::From(pushDir);
	int pushX = fromX + offset.x;
	int pushZ = fromZ + offset.z;

	bool isBlocked = !map->IsWalkable(pushX, pushZ);
	Tile* nextTile = map->GetTile(pushX, pushZ);
	if (nextTile && nextTile->occupant && nextTile->occupant != ignoreOccupant) isBlocked = true;

	if (isBlocked) {
		// 壁や他のユニットに衝突する場合
		return collisionDamage;
	}
	// 衝突せずスムーズに押し出される場合、足元の未発動の罠を確認
	if (Trap* trap = Trap::GetArmedTrap(nextTile)) {
		return trap->GetTrapDamage();
	}
	return 0;
}

int Unit::CalculateExpectedDamage(int baseDamage, bool isPush, Direction pushDir) {
	int expectedDamage = baseDamage;

	// 押し出し攻撃の場合、移動先の状況によって追加の連鎖ダメージを予測する
	if (isPush && m_context && m_context->GetMapManager()) {
		expectedDamage += SimulatePushChainDamage(
			m_context->GetMapManager(), m_gridX, m_gridZ, pushDir, m_onPushDamage);
	}
	return expectedDamage;
}

bool Unit::IsValidMoveTarget(int targetX, int targetZ) {
	if (m_context->GetMapManager() == nullptr) return false;
	return m_context->GetMapManager()->IsWalkable(targetX, targetZ);
}

void Unit::OnPushed(Direction pushDir, Unit* attacker) {
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

		TakeDamage(m_onPushDamage, attacker);
		if (obstacleUnit) {
			// 連鎖衝突：ぶつかられたユニットもダメージを受ける
			obstacleUnit->TakeDamage(m_onPushDamage, attacker);
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
	if (m_facing == newDir) return;

	m_facing = newDir;
	DirOffset o = DirOffset::From(newDir);
	m_targetRot.y = atan2f((float)o.x, (float)o.z) + MODEL_FORWARD_OFFSET;
	m_isTurning = true;
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
	Renderer::SetWorldMatrix(&m_worldMatrix);

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

	// ノックバックの方向に応じて、回転軸と回転方向を決定する
	float dx = m_slideEndPos.x - m_slideStartPos.x;
	float dz = m_slideEndPos.z - m_slideStartPos.z;
	if (fabsf(dz) >= fabsf(dx)) {        

		m_slideTumbleSign = (dz >= 0.0f) ? 1.0f : -1.0f;
	}
	else {                               
		m_slideTumbleOnX = false;
		m_slideTumbleSign = (dx >= 0.0f) ? 1.0f : -1.0f;
	}
}

bool Unit::UpdateSlideAnimation(uint64_t dt) {
	float deltaSeconds = dt / 1000.0f;
	m_slideTimer += deltaSeconds;

	if (m_slideTimer < TIME_SLIDE) {
		float t = m_slideTimer / TIME_SLIDE;
		// EaseOutQuadの適用:「最初は速く、停止直前はゆっくり」という自然な減速感を作る
		float tEase = 1.0f - std::pow(1.0f - t, 2.0f);

		Vector3 p = Vector3::Lerp(m_slideStartPos, m_slideEndPos, tEase);
		//放物線を描くようにY座標を調整することで、ノックバック中の浮き上がりと落下を表現
		//t=0/1は常に0->落下点は影響されない
		p.y += SLIDE_ARC_HEIGHT * 4.0f * t * (1.0f - t);
		m_srt.pos = p;
		// 回転アニメーションの計算：t増加ともに、整数回転、着地時に元の角度に戻るようにする
		float angle = m_slideTumbleSign * SLIDE_TUMBLE_TURNS * 2.0f * PI * t;
		if (m_slideTumbleOnX) m_srt.rot.x = angle;
		else                  m_srt.rot.z = angle;

		UpdateWorldMatrix();
		Renderer::SetWorldMatrix(&m_worldMatrix);
		return false;
	}
	else {
		m_srt.pos = m_slideEndPos;
		m_srt.rot.x = 0.0f;   // 意外な残留回転を防ぐため、X軸とZ軸の回転をリセット
		m_srt.rot.z = 0.0f;
		UpdateWorldMatrix();
		Renderer::SetWorldMatrix(&m_worldMatrix);
		return true;
	}
}

void Unit::UpdateFacingRotation(float dt) {   
	if (!m_isTurning) return;
	float diff = m_targetRot.y - m_srt.rot.y;
	while (diff > PI) diff -= 2.0f * PI;     
	while (diff < -PI) diff += 2.0f * PI;
	if (fabsf(diff) < 0.001f) { m_srt.rot.y = m_targetRot.y; m_isTurning = false; return; }
	m_srt.rot.y += diff * (1.0f - expf(-FACING_LERP_SPEED * dt));
}

// ---------------------------------------------------------

// レンダリングとUI (Rendering & UI)

// ---------------------------------------------------------

void Unit::SetModelRenderer(CStaticMeshRenderer* r) {
	m_renderer = r;

	m_srt.scale = Vector3(1.0f, 1.0f, 1.0f);
	m_srt.rot = Vector3(0.0f, 0.0f, 0.0f);
}

void Unit::DrawModel() {
	if (!m_renderer) return;
	if (!m_isDeathFlying) DrawBlobShadow();
	Renderer::SetWorldMatrix(&m_worldMatrix);
	m_renderer->Draw();
	DrawOutline();
}

void Unit::DrawOutline()
{
	if (Renderer::GetToonParam().OutlineColor.w <= 0.0001f) return;  // 幅0 = 無効化（不要な1回分のDrawを省略）
	if (!m_outlineShader) m_outlineShader = MeshManager::GetShader<CShader>("outlineshader");
	if (!m_toonShader)    m_toonShader = MeshManager::GetShader<CShader>("toonshader");
	if (!m_outlineShader || !m_toonShader) return;

	m_outlineShader->SetGPU();          // アウトライン用VS/PSへ切り替え
	Renderer::SetCullFront();   // 表面をカリングし、裏面シェルのみ描画
	m_renderer->Draw();         // 同じmeshを外側へ拡張して再描画

	// 復元：トゥーンシェーディング + 通常の裏面カリング
	m_toonShader->SetGPU();
	Renderer::DisableCulling(true);  // true = CULL_BACK
}

void Unit::DrawBlobShadow() {
	if (!Renderer::s_shadowEnabled) return;
	Vector3 p = m_srt.pos;
	p.y = ZFight::Blob;  // 地面から少し浮かせる：z-fight防止（Ghostの0.015と同じ考え方）
	Matrix4x4 w = Matrix4x4::CreateScale(BLOB_SIZE, 1.0f, BLOB_SIZE)
		* Matrix4x4::CreateTranslation(p);
	Renderer::SetWorldMatrix(&w);

	if (!m_blobShader) m_blobShader = MeshManager::GetShader<CShader>("blobshader");
	if (!m_blobMesh)   m_blobMesh = MeshManager::GetRenderer<CStaticMeshRenderer>("range_panel_mesh");
	if (!m_toonShader) m_toonShader = MeshManager::GetShader<CShader>("toonshader");
	if (!m_blobShader || !m_blobMesh || !m_toonShader) return;

	m_blobShader->SetGPU();
	Renderer::SetBlendState(BS_ALPHABLEND);
	Renderer::DisableCulling(false);
	Renderer::SetDepthReadOnly();
	m_blobMesh->Draw();

	// 復元：トゥーン + ブレンド無効
	Renderer::SetDepthEnable(true); 
	Renderer::DisableCulling(true);
	Renderer::SetBlendState(BS_NONE);
	m_toonShader->SetGPU();
}

void Unit::DrawUI() {
	if (!m_hpBar || m_currentHP <= 0) return;

	m_hpBar->Draw(m_srt.pos, m_currentHP, m_maxHP, m_previewDamage);

	// 描画後にプレビューダメージを即座にリセットすることで、
	
	// ターゲットされている（エイムされている）フレームのみUIが点滅するように制御する
	m_previewDamage = 0;
}

void Unit::DrawPushPreview(Direction pushDir) {
	if (!m_pushArrowMesh) m_pushArrowMesh = MeshManager::GetRenderer<CStaticMeshRenderer>("arrow_push_mesh");
	if (!m_pushArrowMesh || !m_context || !m_context->GetMapManager()) return;

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
	Vector3 arrowPos = myPos + (targetPos - myPos) * PUSH_ARROW_POS_RATIO;
	arrowPos.y += ZFight::Arrow;

	float rotY = 0.0f;
	if (offset.x == 1)       rotY = 0.0f;
	else if (offset.x == -1) rotY = PI;
	else if (offset.z == 1)  rotY = -PI / 2.0f;
	else if (offset.z == -1) rotY = PI / 2.0f;

	Matrix4x4 world = Matrix4x4::CreateScale(PUSH_ARROW_SCALE)
		* Matrix4x4::CreateRotationY(rotY)
		* Matrix4x4::CreateTranslation(arrowPos);

	// 衝突が予測される場合は黄色、安全に移動できる場合は灰色で視覚的フィードバックを提供する
	Color arrowColor = isBlocked ? PUSH_ARROW_WARN_COLOR : PUSH_ARROW_SAFE_COLOR;

	Renderer::SetWorldMatrix(&world);
	Renderer::DisableCulling(false);
	if (auto* mat = m_pushArrowMesh->GetMaterial(0)) {
		MATERIAL old = mat->GetData();
		MATERIAL temp = old;
		temp.Diffuse = arrowColor;
		mat->SetMaterial(temp);
		m_pushArrowMesh->Draw();
		mat->SetMaterial(old);
	}
	Renderer::DisableCulling(true);

	if (isBlocked && m_context->GetEffectManager()) {
		Vector3 effectPos = targetPos;
		effectPos.y += HIT_EFFECT_Y_OFFSET;
		m_context->GetEffectManager()->DrawStaticHitPreview(effectPos);
	}
}

void Unit::StartDeathFly() {
	m_isDeathFlying = true;
	Vector3 diff = m_srt.pos - m_hitSourcePos;
	diff.y = 0.0f;
	if (diff.LengthSquared() > 0.001f) diff.Normalize();
	else diff = Vector3(0, 0, 1);

	Vector3 flyDir = Vector3(diff.x * DEATH_FLY_DIR_XZ, DEATH_FLY_DIR_Y, diff.z * DEATH_FLY_DIR_XZ);
	m_deathVelocity = flyDir * DEATH_FLY_FORCE;
	auto& rng = RandomEngine::tls();
	m_deathSpin = Vector3(
		static_cast<float>(rng.uniformReal(0.0, DEATH_SPIN_MAX)),
		static_cast<float>(rng.uniformReal(0.0, DEATH_SPIN_MAX)),
		static_cast<float>(rng.uniformReal(0.0, DEATH_SPIN_MAX)));
}

void Unit::UpdateDeathFly(float delta) {
	if (m_isDead) return;

	Camera* cam = (m_context) ? m_context->GetCamera() : nullptr;

	// ① スロー演出の影響を打ち消し、飛翔は常に実時間で進める
	if (cam) {
		float scale = cam->GetTimeScale();
		if (scale > 0.0001f) delta /= scale;
	}

	// ② 物理更新（重力→速度→位置・回転）
	m_deathVelocity.y -= DEATH_GRAVITY * delta;
	m_srt.pos += m_deathVelocity * delta;
	m_srt.rot += m_deathSpin * delta; 

	if (m_srt.pos.y < DEATH_FALL_KILL_Y) OnDeathFlyComplete();
	UpdateWorldMatrix();
}

void Unit::OnDeathFlyComplete() {
	Destroy();
}