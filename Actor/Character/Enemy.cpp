#include "Enemy.h"    
#include "../../System/MeshManager.h"
#include "../../System/ZFightTunables.h"
#include "../../System/RandomEngine.h"
#include "../../System/ModelRegistry.h"
#include "../../Core/GameContext.h"
#include "../../GamePlay/Scene/GameScene.h"
#include "../../GamePlay/Manager/MapManager.h"
#include "../../UI/System/DamageNumberManager.h"
#include "../../GamePlay/Manager/EffectManager.h"
#include "../../GamePlay/Manager/EnemyManager.h"
#include "../../Actor/Character/Player.h"
#include "../../Actor/Character/Ally.h"
#include "../../System/FxTunables.h"
#include "../../System/collision.h"
#include "../../System/ForecastTunables.h"


namespace {
	// 演出・バランス用定数
	const int INITIAL_HP = 5;
	const int INITIAL_MOVE_POINTS = 4;
	const float MOVE_SPEED = 5.0f;
	const float CHARGE_SHAKE_AMP = 0.01f;      // 蓄力時の震え幅
	const float ATTACK_DELAY = 0.1f;           // 攻撃開始前のディレイ
	const float MODEL_SCALE = 0.8f;            // 敵モデルの表示スケール

	const int DIST_UNREACHABLE = 999;          // ターゲット不在時の距離初期値（十分大きな値）
	const float AXIS_BIAS_THRESHOLD = 0.1f;    // 方向判定でX/Z軸を区別する最小差分

	const float MISS_NUM_Y_OFFSET = 1.0f;      // 空振り（miss）数字の表示高さ

	// --- 床ヒントUIの表示色 ---
	const Color DANGER_TILE_COLOR = Color(0.9f, 0.0f, 0.0f, 0.5f);  // ロックオン中のタイル：半透明の赤

	const float ENEMY_WARN_SIZE = 1.0f;   // 攻撃予警範囲（矩形）の一辺（プレイヤーの AIM_WARN_SIZE と対称）
	const float ENEMY_WARN_OFFSET = 1.0f;   // 自分の正面方向への配置距離
	const Color ENEMY_WARN_COLOR = Color(1.0f, 0.2f, 0.2f, 0.25f); // 薄い赤（危険範囲）

}

//Spawnファクトリー
std::unique_ptr<Enemy> Enemy::Spawn(GameContext* ctx, int gridX, int gridZ, const Vector3& worldPos) {
	auto e = std::unique_ptr<Enemy>(new Enemy(ctx));
	e->Init();
	e->SetGridPosition(gridX, gridZ);
	e->SetPosition(worldPos);
	e->UpdateWorldMatrix();
	return e;
}

void Enemy::Init() {
	SetModelRenderer(ModelRegistry::RegisterModel(
		"enemy_mesh", "Assets/model/character/Cat/Cat_01.obj", "Assets/model/character/Cat"));
	m_enemyShader = MeshManager::GetShader<CShader>("toonshader");

	m_actionUI = std::make_unique<EnemyActionUI>();
	m_actionUI->Init(m_context);

	m_srt.scale = Vector3(MODEL_SCALE, MODEL_SCALE, MODEL_SCALE);
	m_srt.rot = Vector3(0, 0, 0);
	m_moveSpeed = MOVE_SPEED;

	m_maxMovePoints = INITIAL_MOVE_POINTS;
	m_currentMovePoints = m_maxMovePoints;
	m_maxHP = INITIAL_HP;
	m_currentHP = m_maxHP;
	m_state = EnemyState::IDLE;
	m_isDead = false;

	UpdateWorldMatrix();
}


void Enemy::Update(float deltaSeconds) {
	Unit::Update(deltaSeconds);

	UpdateFacingRotation(deltaSeconds);

	if (m_pendingCharge && !m_isTurning) {
		// 蓄力（突進準備）が保留されており、かつ反転アニメーションが完了（false）した場合
		m_pendingCharge = false;
		m_isCharging = true;
	}

	// 蓄力（チャージ）中の震えオフセットを計算
	if (m_isCharging) {
		auto& rng = RandomEngine::tls();
		m_shakeOffset = Vector3(
			static_cast<float>(rng.uniformReal(-CHARGE_SHAKE_AMP, CHARGE_SHAKE_AMP)), 0.0f,
			static_cast<float>(rng.uniformReal(-CHARGE_SHAKE_AMP, CHARGE_SHAKE_AMP)));
	}
	else {
		m_shakeOffset = Vector3(0, 0, 0);
	}

	// 死亡確定演出：その場で震え、時間経過でバーストと共に飛翔開始
	if (m_state == EnemyState::DEATH_SHAKE) {
		// スロー演出の影響を打ち消し、実時間で進行（UpdateDeathFly と同方針）
		float dt = deltaSeconds;
		if (Camera* cam = m_context ? m_context->GetCamera() : nullptr) {
			float s = cam->GetTimeScale();
			if (s > 0.0001f) dt /= s;
		}

		auto& rng = RandomEngine::tls();
		m_shakeOffset = Vector3(
			(float)rng.uniformReal(-Fx::DeathShake.amp, Fx::DeathShake.amp), 0.0f,
			(float)rng.uniformReal(-Fx::DeathShake.amp, Fx::DeathShake.amp));

		m_deathShakeTimer -= dt;
		if (m_deathShakeTimer <= 0.0f) {
			m_shakeOffset = Vector3(0, 0, 0);

			StartDeathFly();
			m_state = EnemyState::DEAD_FLYING;
		}
		UpdateWorldMatrix();
		return;
	}

	//死亡飛翔中の更新処理
	if (m_state == EnemyState::DEAD_FLYING)
	{
		DeathFlyingUpdate(deltaSeconds);
		return; // これ以降のAIや移動処理はさせない
	}

	if (m_isMyTurn && m_state == EnemyState::IDLE) {
		ExecuteAI();
		m_isMyTurn = false;
	}

	if (m_actionUI) m_actionUI->Update(deltaSeconds);

	// 敵のダメージ予測を計算し、ロック対象へ設定
	if (m_isCharging && WillHitLockedVictim()
		&& !m_isDead && m_state != EnemyState::DEAD_FLYING) {
		Vector3 pushDir = m_lockedVictim->GetSRT().pos - m_srt.pos; pushDir.y = 0.0f;
		int finalDmg = m_lockedVictim->CalculateExpectedDamage(m_enemyDamage, true, pushDir);
		m_lockedVictim->SetPreviewDamage(finalDmg);
	}

	switch (m_state) {
	case EnemyState::MOVING:
		if (DriveTowardVictim(deltaSeconds)) OnMoveFinished();
		break;

	case EnemyState::ATTACKING:

		m_attackTimer += deltaSeconds;
		if (m_attackTimer > ATTACK_DELAY) {
			auto impactCallback = [this]() {
				if (WillHitLockedVictim()) {
					Unit* victim = m_lockedVictim;
					Vector3 pushDir = victim->GetSRT().pos - m_srt.pos; pushDir.y = 0.0f;
					victim->OnPushed(pushDir, this);      // 連続方向（予測と一致）
					victim->TakeDamage(m_enemyDamage, this);
				}
				else { // miss
					if (m_context->GetDamageManager()) {
						Vector3 missPos = m_lockedVictim ? m_lockedVictim->GetSRT().pos : m_srt.pos;
						missPos.y += MISS_NUM_Y_OFFSET;
						m_context->GetDamageManager()->SpawnDamage(missPos, 0);
					}
				}
				};

			if (UpdateAttackAnimation(deltaSeconds, impactCallback)) {
				//アタックアニメ実行、完了の検査
				m_state = EnemyState::IDLE;
				m_isCharging = false;
				EnemyEndAction();
			}
		}
		break;

	case EnemyState::KNOCKBACK:
		break;
	default:break;
	}

	UpdateWorldMatrix();
}

void Enemy::OnDraw(float /*deltaSeconds*/) {

	if (m_enemyShader != nullptr) m_enemyShader->SetGPU();

	// 蓄力中の震えは `m_srt.pos` を直接汚染せず、描画用の一時的な Matrix で処理する
		if (m_isCharging || m_state == EnemyState::DEATH_SHAKE) {
			if (m_fade >= 0.999f) return;
		Matrix4x4 shakeWorld = Matrix4x4::CreateScale(m_srt.scale)
			* Matrix4x4::CreateRotationY(m_srt.rot.y)
			* Matrix4x4::CreateTranslation(m_srt.pos + m_shakeOffset); // オフセットを加算
		Renderer::SetWorldMatrix(&shakeWorld);
		if (m_renderer) m_renderer->Draw();
	}
	else {
		DrawModel();
	}

}

void Enemy::SetInitialFacingToPlayer() {
	Player* target = m_context->GetPlayer();
	if (target)
	{
		Vector3 myPos = GetMap()->GetWorldPosition(m_gridX, m_gridZ);
		Vector3 targetPos = GetMap()->GetWorldPosition(target->GetUnitGridX(), target->GetUnitGridZ());

		// ベクトル：自分 -> ターゲット (targetPos - myPos = 前進方向)
		Vector3 diff = targetPos - myPos;

		Direction finalDir = Direction::South;

		// --- 【重要】水平方向優先 (Horizontal Bias) の判定 ---
		// 通常、斜め（例：-2, -3）の場合は数値の大きい South が判定されるが、
		// 視覚的にはプレイヤーを側面的に捉えた方が自然なため、X軸に重みを持たせる。
		if (std::abs(diff.x) > AXIS_BIAS_THRESHOLD)
		{
			// 東・西を優先判定
			finalDir = (diff.x > 0) ? Direction::East : Direction::West;
		}
		else
		{
			// 角度が垂直に近い場合のみ、北・南と判定
			finalDir = (diff.z > 0) ? Direction::North : Direction::South;
		}

		// 3. 向きの適用 (アニメーションのトリガー)
		// finalDir が West なら「South -> West」の反転アニメーションが開始される
		// finalDir が South の場合は静止を維持する（仕様通り）
		SetFacing(finalDir);

		m_srt.rot.y = m_targetRot.y;
		UpdateWorldMatrix();
	}
}

void Enemy::EnemyStartAction() {
	if (m_currentHP <= 0 || m_state == EnemyState::DEAD_FLYING) return;

	if (m_isCharging) {
		ReleaseChargeAttack();
	}

	else {
		m_pendingCharge = false;
		ResetMovePoints();
		m_moveOrigin = m_srt.pos;// 移動予算円の中心
		m_moveBudget = (float)m_maxMovePoints * 1.0f;
		ExecuteAI();
	}

}

void Enemy::OnTurnChanged(TurnState state) {
	if (state == TurnState::EnemyPhase) StartTurn();
}

void Enemy::OnKnockbackBegin() {
	m_state = EnemyState::KNOCKBACK;
}
void Enemy::OnKnockbackEnd() {
	if (m_currentHP <= 0) { Die(); return; }   // 滑り終わってから死亡演出へ
	m_state = EnemyState::IDLE;
}

void Enemy::TakeDamage(int damage, Unit* attacker) {
	Unit::TakeDamage(damage, attacker);
	if (attacker) m_hitSourcePos = attacker->GetSRT().pos;

	if (m_currentHP <= 0 && !IsDeadFlying())
	{
		// 現在ノックバック中（KNOCKBACK）であれば、死亡演出を遅延させる
		if (m_state == EnemyState::KNOCKBACK) {
			return;
		}
		// それ以外の場合は、即座に死亡処理を実行
		Die();
	}
}

void Enemy::ExecuteAI() {
	Player* player = m_context->GetPlayer();
	Ally* ally = m_context->GetAlly();

	//ターゲット決定(一番近いユニット)
	int distToPlayer = DIST_UNREACHABLE;
	int distToAlly = DIST_UNREACHABLE;

	//プレイヤーの距離計算
	if (this->CanTarget(player) && !player->IsInvincible()) {
		distToPlayer = GetMap()->CalculateDistance(
			this->m_gridX, this->m_gridZ, player->GetUnitGridX(), player->GetUnitGridZ());
	}

	//味方の距離計算
	if (this->CanTarget(ally) && !ally->IsInvincible()) {
		distToAlly = GetMap()->CalculateDistance(
			this->m_gridX, this->m_gridZ, ally->GetUnitGridX(), ally->GetUnitGridZ());
	}

	// 近い方を第一候補、もう一方を第二候補とする（同距離はプレイヤー優先）。
	// 第一候補が到達不可（経路が塞がれている等）の場合、第二候補へフォールバックする
	Unit* primary = (distToAlly < distToPlayer) ? (Unit*)ally : (Unit*)player;
	Unit* secondary = (primary == (Unit*)ally) ? (Unit*)player : (Unit*)ally;

	int primaryDist = (primary == (Unit*)ally) ? distToAlly : distToPlayer;
	int secondaryDist = (primary == (Unit*)ally) ? distToPlayer : distToAlly;

	if (primaryDist < DIST_UNREACHABLE && TryActOnTarget(primary)) return;
	if (secondaryDist < DIST_UNREACHABLE && TryActOnTarget(secondary)) return;

	// 全候補に対して行動不能：その場でターン終了
	EnemyEndAction();
}

bool Enemy::TryActOnTarget(Unit* target) {
	if (!target) return false;
	// すでに幾何的に攻撃可能なら → チャージ
	if (WouldHitVictim(target)) { StartCharge(target); return true; }
	// それ以外は連続接近を開始（移動予算内で可能な限り接近）
	m_moveTarget = target;
	m_state = EnemyState::MOVING;
	// 連続移動中はグリッド占有を解除
	if (Tile* t = GetMap()->GetTile(m_gridX, m_gridZ))
		if (t->occupant == this) t->occupant = nullptr;
	return true;
}


bool Enemy::WouldHitVictim(Unit* v) const {
	if (!CanTarget(v)) return false;
	using namespace GM31::GE::Collision;
	Vector3 dir = v->GetSRT().pos - m_srt.pos; dir.y = 0.0f;
	if (dir.LengthSquared() < 1e-6f) dir = Vector3(sinf(m_srt.rot.y), 0.0f, cosf(m_srt.rot.y));
	dir.Normalize();
	float yaw = atan2f(dir.x, dir.z);
	Vector3 center = m_srt.pos + dir * ENEMY_WARN_OFFSET;
	BoundingBoxOBB obb = SetOBB(Vector3(0.0f, yaw, 0.0f), center,
		ENEMY_WARN_SIZE, 2.0f, ENEMY_WARN_SIZE);
	BoundingSphere sph; sph.center = v->GetSRT().pos; sph.radius = ForecastUI::HitRingRadius;
	return CollisionSphereOBB(sph, obb);
}
bool Enemy::WillHitLockedVictim() const { return WouldHitVictim(m_lockedVictim); }


void Enemy::EnemyEndAction() {
	m_state = EnemyState::IDLE;
}

void Enemy::GetAttackBox(Vector3& center, float& yaw) const {
	Vector3 dir = m_lockedVictim ? (m_lockedVictim->GetSRT().pos - m_srt.pos) : Vector3(0, 0, 0);
	dir.y = 0.0f;
	if (dir.LengthSquared() < 1e-6f) dir = Vector3(sinf(m_srt.rot.y), 0.0f, cosf(m_srt.rot.y));
	dir.Normalize();
	yaw = atan2f(dir.x, dir.z);                     // fwd=(sin yaw,0,cos yaw)
	center = m_srt.pos + dir * ENEMY_WARN_OFFSET;
}

bool Enemy::DriveTowardVictim(float dt) {
	if (!m_moveTarget) return true;
	Vector3 toV = m_moveTarget->GetSRT().pos - m_srt.pos; toV.y = 0.0f;
	float distToV = toV.Length();
	float contact = m_bodyRadius + m_moveTarget->GetBodyRadius();
	if (distToV <= contact + 0.02f) return true;   // 接触＝これ以上接近できない

	Vector3 dir = toV / distToV;
	Vector3 before = m_srt.pos;

	Vector3 newPos = m_srt.pos + dir * (m_moveSpeed * dt);
	newPos = GetMap()->ResolveCircleCollision(newPos, m_bodyRadius);   // 壁に沿って移動
	newPos = ResolveUnitCollision(newPos);// 他ユニットを回避
	Vector3 clamped = ClampToMoveCircle(newPos);
	bool atBudget = (clamped - newPos).LengthSquared() > 1e-8f;
	newPos = clamped;
	newPos.y = m_srt.pos.y;
	m_srt.pos = newPos;
	SetFacingYaw(atan2f(dir.x, dir.z));
	SyncGridFromWorld();
	UpdateWorldMatrix();

	if (atBudget) return true;                         // 移動予算を使い切った
	Vector3 moved = m_srt.pos - before; moved.y = 0.0f;
	if (moved.LengthSquared() < 1e-8f) return true;    // 壁に阻まれて進めない＝これ以上進めない
	return false;
}

void Enemy::SyncGridFromWorld() {
	int gx, gz;
	if (GetMap() && GetMap()->WorldToGrid(m_srt.pos, gx, gz)) { m_gridX = gx; m_gridZ = gz; }
}

void Enemy::OnMoveFinished() {
	m_state = EnemyState::IDLE;
	SyncGridFromWorld();                          // 連続座標をグリッドへ反映
	if (Tile* t = GetMap()->GetTile(m_gridX, m_gridZ)) {
		t->occupant = this;
		if (t->structure) t->structure->OnEnter(this);   // 足元の罠
	}
	// 接近後、幾何的に攻撃可能ならチャージ／攻撃できなければターン終了
	if (m_moveTarget && WouldHitVictim(m_moveTarget)) StartCharge(m_moveTarget);
	else EnemyEndAction();
	m_moveTarget = nullptr;
}

void Enemy::StartCharge(Unit * target) {
	if (target) {
		m_lockedVictim = target;
		m_isTurning = false;   // 残っていたフリップを中断し、SetFacing を確実に反映
		SetFacingFromVector(target->GetSRT().pos - m_srt.pos);   // 連続座標で向きを合わせる
	}

	if (m_isTurning) {
		// 反転アニメーション中の場合は、突進前の「震え演出」を保留にする
		m_pendingCharge = true;
		m_isCharging = false;
	}
	else {
		// 既に対象の方向を向いている（反転不要な）場合は、即座に震え演出を開始
		m_pendingCharge = false;
		m_isCharging = true;
	}
	EnemyEndAction();
}


void Enemy::Die() {
	m_state = EnemyState::DEATH_SHAKE;
	m_deathShakeTimer = Fx::DeathShake.duration;
	// 死亡時に蓄力（チャージ）状態を強制クリアし、警告UIの残存を防止する
	m_isCharging = false;
	m_pendingCharge = false;
	m_lockedVictim = nullptr;

	if (m_context && GetMap()) {
		Tile* myTile = GetMap()->GetTile(m_gridX, m_gridZ);
		if (myTile && myTile->occupant == this) myTile->occupant = nullptr;
	}

	// 死亡の瞬間：震えと同時に多色バーストを爆散させる
	if (GetEffectManager()) {
		Vector3 burstPos = m_srt.pos;
		burstPos.y += Fx::Burst.spawnYOffset;
		GetEffectManager()->Spawn3DDeathBurst(burstPos);
	}

	// カメラは「震え→バースト→飛翔」を通しで見せる
	if (m_context && m_context->GetCamera())
		m_context->GetCamera()->PlayKillCam(m_hitSourcePos, m_srt.pos, true);
}

void Enemy::DeathFlyingUpdate(float deltaSeconds) {
	Unit::UpdateDeathFly(deltaSeconds);
}

void Enemy::OnDeathFlyComplete() {
	if (m_context && m_context->GetEnemyManager())
		m_context->GetEnemyManager()->RemoveEnemy(this);
	Destroy();
}

void Enemy::DrawUI() {
	Unit::DrawUI();
	if (m_currentHP <= 0) return;
	if (m_actionUI) {
		m_actionUI->Draw(m_srt.pos, m_displayOrder);
	}
}

void Enemy::OnDrawFloorUI(float /*deltaSeconds*/) {
	if (m_currentHP <= 0) return;

	// 蓄力中：攻撃予警範囲（矩形・危険範囲）＋被弾円＋着地点の円を床レイヤーに描画
	if (m_isCharging && CanTarget(m_lockedVictim)) {
		Vector3 c; float yaw; GetAttackBox(c, yaw);
		DrawWarningBox(c, yaw, ENEMY_WARN_SIZE, ENEMY_WARN_COLOR);   // 危険範囲（常に表示）
		DrawHitRing(m_lockedVictim);                                  // 被弾円（常に表示）
		if (WillHitLockedVictim()) DrawLandingRing(m_lockedVictim);   // 命中見込み時のみ
	}

	// 移動中：移動可能円（予算）を表示（敵は赤系）
	if (m_state == EnemyState::MOVING) DrawMoveRangeCircle();
}

void Enemy::OnDrawOverlay(float /*deltaSeconds*/) {
	if (m_currentHP <= 0) return;

	// 蓄力中：ロック対象への放物線状の予測矢印を最前面に描画（敵／罠に遮られない）
	if (m_isCharging && WillHitLockedVictim()) {
		DrawForecastArrow(m_lockedVictim);
	}
}

void Enemy::ReleaseChargeAttack() {
	m_state = EnemyState::ATTACKING;
	Vector3 targetGridPos = m_lockedVictim ? m_lockedVictim->GetSRT().pos : m_srt.pos;
	StartAttackAnimation(targetGridPos);
	m_attackTimer = 0.0f;
	m_isCharging = false;

	if (m_context && m_context->GetCamera())
		m_context->GetCamera()->PlayAttackZoom((m_srt.pos + targetGridPos) * 0.5f);
}

void Enemy::PlayActionOrderBounce(int times) { if (m_actionUI) m_actionUI->PlayBounce(times); }
bool Enemy::IsActionOrderBouncing() const { return m_actionUI && m_actionUI->IsBouncing(); }