#include "Unit.h"
#include "../../Core/GameContext.h"
#include "../../GamePlay/Manager/MapManager.h"
#include "../../GamePlay/Manager/TurnManager.h"
#include "../../UI/System/DamageNumberManager.h"
#include "../../GamePlay/Manager/EffectManager.h"
#include "../../Actor/Gimmick/Trap.h"
#include "../../System/ZFightTunables.h"
#include "../../System/ModelRegistry.h"
#include <cmath>
#include "../../System/RandomEngine.h"
#include "../../System/FxTunables.h"
#include "../../System/CMaterial.h"
#include "../../System/collision.h"
#include "../../GamePlay/Manager/EnemyManager.h"
#include "../Character/Ally.h"
#include "../../System/ForecastTunables.h"
#include "../Character/Player.h"
#include "../Character/Enemy.h"

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

	const float FADE_LERP_SPEED = 10.0f; 

	const float PUSH_DIST = 1.0f;   // 押し出し距離（1格固定）


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
}

Unit::Unit(GameContext* context) : GameObject(context) {
	if (m_context && GetTurnManager()) {
		m_turnConnection = GetTurnManager()->RegisterObserver(
			[this](TurnState state) { this->OnTurnChanged(state); }
		);
	}

	m_hpBar = std::make_unique<HPBar>();
	m_hpBar->Init(context);
}

Unit::~Unit() {}

// ---------------------------------------------------------

// ライフサイクル (Lifecycle)

// ---------------------------------------------------------

void Unit::Update(float deltaSeconds) {
	if (m_hpBar) {
		m_hpBar->Update(deltaSeconds);
	}

	// fade を目標値へ滑らかに遷移（遮蔽フェード / 接近フェードで共通利用）
	if (m_fade != m_targetFade) {
		float t = 1.0f - expf(-FADE_LERP_SPEED * deltaSeconds);
		m_fade += (m_targetFade - m_fade) * t;
		if (fabsf(m_fade - m_targetFade) < 0.001f) m_fade = m_targetFade;
	}

	if (m_isKnockback) UpdateKnockback(deltaSeconds);
}

void Unit::StartTurn() {}
void Unit::EndTurn() {}
void Unit::OnTurnChanged(TurnState state) {}

// ---------------------------------------------------------

// 戦闘と物理干渉 (Combat & Physics)

// ---------------------------------------------------------

void Unit::TakeDamage(int damage, Unit* attacker) {
	if (m_isInvincible) return;
	if (damage < 0) return;
	m_currentHP = std::max(0, m_currentHP - damage);
	if (attacker) m_hitSourcePos = attacker->GetSRT().pos;

	// 視覚的重複の回避：複数回ダメージを受けた際、エフェクトや数字が
	
	// 完全に重なって見えなくなるのを防ぐため、ランダムなオフセットを加える
	if (m_context && GetEffectManager()) {
		auto& rng = RandomEngine::tls();
		Vector3 hitPos = m_srt.pos;
		hitPos.y += HIT_EFFECT_Y_OFFSET;
		hitPos.x += static_cast<float>(rng.uniformReal(-0.5, 0.5)) * HIT_POS_RANDOM_SPREAD;
		hitPos.z += static_cast<float>(rng.uniformReal(-0.5, 0.5)) * HIT_POS_RANDOM_SPREAD;

		GetEffectManager()->Spawn3DHit(hitPos);
	}

	if (m_context && m_context->GetDamageManager()) {
		Vector3 headPos = m_srt.pos;
		headPos.y += DAMAGE_NUM_Y_OFFSET;
		headPos.x += static_cast<float>(RandomEngine::tls().uniformReal(-0.5, 0.5)) * DAMAGE_NUM_RANDOM_SPREAD;

		m_context->GetDamageManager()->SpawnDamage(headPos, damage);
	}

	OnHpChanged();
}

int Unit::CalculateExpectedDamage(int baseDamage, bool isPush, const Vector3& pushDir) {
	int expected = baseDamage;
	if (isPush && m_context) {
		PushResult r = SimulatePush(m_context, m_srt.pos, pushDir, PUSH_DIST,
			GetBodyRadius(), m_onPushDamage, this);
		expected += r.chainDamage;
	}
	return expected;
}

bool Unit::IsValidMoveTarget(int targetX, int targetZ) {
	if (GetMap() == nullptr) return false;
	return GetMap()->IsWalkable(targetX, targetZ);
}

void Unit::OnPushed(const Vector3& pushDir, Unit* attacker) {
	if (!CanBePushed()) return;
	if (attacker) m_hitSourcePos = attacker->GetSRT().pos;

	OnKnockbackBegin();   // 派生：被击退状態＋固有処理

	PushResult r = SimulatePush(m_context, m_srt.pos, pushDir, PUSH_DIST,
		GetBodyRadius(), m_onPushDamage, this);
	if (r.blocked) {
		Vector3 dir = pushDir; dir.y = 0.0f;
		if (dir.LengthSquared() > 0.0001f) dir.Normalize(); else dir = Vector3(0, 0, 1);
		StartBumpAnimation(m_srt.pos + dir * PUSH_DIST);
		m_slideEndPos = Vector3(0, 0, 0);       // 位移なし＝滑走なし
		TakeDamage(m_onPushDamage, attacker);
		if (r.hitUnit) r.hitUnit->TakeDamage(m_onPushDamage, attacker);   // 連鎖
	}
	else {
		if (GetMap()) { int gx, gz; if (GetMap()->WorldToGrid(r.landingPos, gx, gz)) { m_gridX = gx; m_gridZ = gz; } }
		StartSlideAnimation(r.landingPos);
	}
	m_isKnockback = true;
}

void Unit::UpdateKnockback(float dt) {
	// 撞击で即死：滑らせず終了へ（罠スキップ）。死亡演出は派生の OnKnockbackEnd→Die に任せる
	if (m_currentHP <= 0) { m_isKnockback = false; OnKnockbackEnd(); return; }

	bool done = (m_slideEndPos.LengthSquared() > 0.001f) ? UpdateSlideAnimation(dt) : true;
	if (!done) return;

	m_isKnockback = false;
	m_slideEndPos = Vector3(0, 0, 0);

	// 生存時のみ落点ギミック（罠）を発火
	if (GetMap()) {
		Tile* t = GetMap()->GetTile(m_gridX, m_gridZ);
		if (t && t->structure) t->structure->OnEnter(this);
	}
	OnKnockbackEnd();   // 派生：通常状態へ
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

void Unit::SetFacingYaw(float yawRadians) {
	// 視覚は連続 yaw（既存の lerp をそのまま利用）
	m_targetRot.y = yawRadians + MODEL_FORWARD_OFFSET;
	m_isTurning = true;

	// 離散 m_facing も最寄り4向へ同期しておく（攻撃の初期方向などロジック用）
	// yaw=atan2(dirX, dirZ) の逆：dir=(sin yaw, cos yaw)
	m_facing = DirOffset::FromVector(sinf(yawRadians), cosf(yawRadians));
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

bool Unit::UpdateAttackAnimation(float deltaSeconds, std::function<void()> onImpact) {
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

bool Unit::UpdateSlideAnimation(float deltaSeconds) {
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
	if (!m_renderer || m_isDeathVisualHidden) return;
	if (m_fade >= 0.999f) return;
	if (!m_isDeathFlying) DrawBlobShadow();

	// fade > 0 の場合、非表示量をマテリアルの Dummy.x に設定。
	// 描画後に元の値へ戻し、共有マテリアルへの影響を残さない。
	const bool fading = (m_fade > 0.001f);
	if (fading) ApplyFadeToMaterials(m_fade);

	Renderer::SetWorldMatrix(&m_worldMatrix);
	m_renderer->Draw();
	DrawOutline();

	if (fading) ApplyFadeToMaterials(0.0f);
}

void Unit::ApplyFadeToMaterials(float fade) {
	// 本体のすべての子マテリアルを走査し、fade 値を Dummy.x に設定
	if (!m_renderer) return;
	for (int i = 0; CMaterial * mtrl = m_renderer->GetMaterial(i); ++i) {
		MATERIAL data = mtrl->GetData();
		data.Dummy[0] = fade;
		mtrl->SetMaterial(data);
	}
}

void Unit::DrawOutline()
{
	// 上書きがあれば専用色、なければグローバル値（.w = 幅、0 = 無効化）
	TOONPARAM globalToon = Renderer::GetToonParam();
	const Color& oc = m_hasOutlineOverride ? m_outlineOverrideColor : globalToon.OutlineColor;
	if (oc.w <= 0.0001f) return;// 幅0 = 無効化（不要な1回分のDrawを省略）
	
	if (!m_outlineShader) m_outlineShader = MeshManager::GetShader<CShader>("outlineshader");
	if (!m_toonShader)    m_toonShader = MeshManager::GetShader<CShader>("toonshader");
	if (!m_outlineShader || !m_toonShader) return;

	// 上書き色を定数バッファへ一時反映
	if (m_hasOutlineOverride) {
		TOONPARAM tmp = globalToon;
		tmp.OutlineColor = m_outlineOverrideColor;
		Renderer::SetToonParam(tmp);
	}

	m_outlineShader->SetGPU();          // アウトライン用VS/PSへ切り替え
	Renderer::SetCullFront();   // 表面をカリングし、裏面シェルのみ描画
	m_renderer->Draw();         // 同じmeshを外側へ拡張して再描画

	// 還元：グローバル値へ戻す
	if (m_hasOutlineOverride) {
		Renderer::SetToonParam(globalToon);
	}

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
	if (!s_hpBarVisible || !m_hpBar || m_currentHP <= 0) return;

	m_hpBar->Draw(m_srt.pos, m_currentHP, m_maxHP, m_previewDamage);

	// 描画後にプレビューダメージを即座にリセットすることで、
	
	// ターゲットされている（エイムされている）フレームのみUIが点滅するように制御する
	m_previewDamage = 0;
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

	// 進行率の基準：上昇と対称な放物線の周期（0.5 = 頂点、0.6 = 頂点を少し過ぎた位置）
	m_deathArcTime = (m_deathVelocity.y > 0.01f)
		? 2.0f * m_deathVelocity.y / DEATH_GRAVITY : 0.5f;
	m_deathFlyTimer = 0.0f;
	m_deathTrailTimer = 0.0f;
	m_deathStarSpawned = false;
	m_isDeathVisualHidden = false;
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

	// ③ 飛翔演出：トレイル生成 → 60% 地点で十字スター＋モデル消滅
	m_deathFlyTimer += delta;
	if (!m_isDeathVisualHidden && GetEffectManager()) {
		m_deathTrailTimer += delta;
		while (m_deathTrailTimer >= Fx::Trail.interval) {
			m_deathTrailTimer -= Fx::Trail.interval;
			GetEffectManager()->Spawn3DTrailPuff(m_srt.pos);
		}
		if (!m_deathStarSpawned && m_deathFlyTimer >= m_deathArcTime * Fx::Star.progress) {
			m_deathStarSpawned = true;
			m_isDeathVisualHidden = true;   // スターと同時にモデル消滅（トレイルも止まる）
			GetEffectManager()->Spawn3DStarCross(m_srt.pos);
		}
	}

	if (m_srt.pos.y < DEATH_FALL_KILL_Y) OnDeathFlyComplete();
	UpdateWorldMatrix();
}

void Unit::OnDeathFlyComplete() {
	Destroy();
}

Unit::PushResult Unit::SimulatePush(GameContext* ctx, const Vector3& fromPos,
	const Vector3& pushDir, float pushDist, float victimRadius,
	int collisionDamage, const Unit* ignoreSelf) {
	using namespace GM31::GE::Collision;
	PushResult r;

	Vector3 dir = pushDir; dir.y = 0.0f;
	if (dir.LengthSquared() < 0.0001f) { r.landingPos = fromPos; return r; }
	dir.Normalize();
	r.landingPos = fromPos + dir * pushDist;

	MapManager* map = ctx ? ctx->GetMapManager() : nullptr;

	// ① 壁：落点円が壁に食い込むなら反弹
	if (map && map->CircleHitsWall(r.landingPos, victimRadius)) {
		r.blocked = true; r.landingPos = fromPos; r.chainDamage = collisionDamage;
		return r;
	}

	// ② ユニット：落点円が他ユニットの円と重なるなら反弹＋連鎖
	std::vector<Unit*> units;
	if (ctx) {
		if (Unit* p = (Unit*)ctx->GetPlayer()) units.push_back(p);
		if (Unit* a = (Unit*)ctx->GetAlly())   units.push_back(a);
		if (auto* em = ctx->GetEnemyManager())
			for (Enemy* e : em->GetAllEnemies()) if (e) units.push_back(e);
	}
	BoundingSphere land{ r.landingPos, victimRadius };
	for (Unit* u : units) {
		if (!u || u == ignoreSelf || u->IsDead()) continue;
		BoundingSphere us{ u->GetSRT().pos, u->GetBodyRadius() };
		if (CollisionSphere(land, us)) {
			r.blocked = true; r.hitUnit = u; r.landingPos = fromPos;
			r.chainDamage = collisionDamage;
			return r;
		}
	}

	// ③ 無阻：落点マスに未発動の罠があれば罠ダメージ
	if (map) {
		int gx, gz;
		if (map->WorldToGrid(r.landingPos, gx, gz)) {
			if (Trap* trap = Trap::GetArmedTrap(map->GetTile(gx, gz)))
				r.chainDamage = trap->GetTrapDamage();
		}
	}
	return r;
}

// ===== ノックバック予測（レイヤー別） =====

// 敵の被弾円：床レイヤーに描画し、その後に描画される敵／罠が上に乗る
void Unit::DrawHitRing(Unit* target) {
	if (!target) return;
	DrawGroundRing(target->GetSRT().pos, ForecastUI::HitRingRadius, ForecastUI::HitRingColor);
}

// 着地点の円：床レイヤー
void Unit::DrawLandingRing(Unit* target) {
	if (!target) return;
	Vector3 dir = target->GetSRT().pos - m_srt.pos; dir.y = 0.0f;
	if (dir.LengthSquared() < 0.0001f) return;
	dir.Normalize();
	PushResult r = SimulatePush(m_context, target->GetSRT().pos, dir,
		PUSH_DIST, target->GetBodyRadius(), 0, target);
	const Color col = r.blocked ? Color(1.0f, 0.3f, 0.2f, 1.0f) : Color(0.4f, 1.0f, 0.5f, 1.0f);
	Vector3 spot = r.blocked ? (target->GetSRT().pos + dir * PUSH_DIST) : r.landingPos;
	DrawGroundRing(spot, ForecastUI::LandingRingRadius, col);
}

// 放物線状の矢印（最前面）＋衝突予測エフェクト
void Unit::DrawForecastArrow(Unit* target) {
	if (!target) return;
	Vector3 dir = target->GetSRT().pos - m_srt.pos; dir.y = 0.0f;
	if (dir.LengthSquared() < 0.0001f) return;
	dir.Normalize();
	PushResult r = SimulatePush(m_context, target->GetSRT().pos, dir,
		PUSH_DIST, target->GetBodyRadius(), 0, target);
	const Color col = r.blocked ? Color(1.0f, 0.3f, 0.2f, 1.0f) : Color(0.4f, 1.0f, 0.5f, 1.0f);
	Vector3 spot = r.blocked ? (target->GetSRT().pos + dir * PUSH_DIST) : r.landingPos;
	DrawArcArrow(target->GetSRT().pos, spot, col);
	if (r.blocked && GetEffectManager()) {
		Vector3 fxPos = spot; fxPos.y += HIT_EFFECT_Y_OFFSET;
		GetEffectManager()->DrawStaticHitPreview(fxPos);
	}
}

// 円（塗り＋スムーズなライン）。プレイヤーの移動範囲円（DrawActionCircle/Line）と同じメッシュ・同じ方式。
// 床レイヤー用：depth-read-only で地面に貼り、敵／罠が上に乗る（踏まれる）
void Unit::DrawGroundRing(const Vector3 & center, float radius, const Color & color) {
	CStaticMeshRenderer* fill = MeshManager::GetRenderer<CStaticMeshRenderer>("action_ring_mesh");
	CStaticMeshRenderer* line = MeshManager::GetRenderer<CStaticMeshRenderer>("action_ring_line_attack_mesh");
	CShader* fx = MeshManager::GetShader<CShader>("fxshader");
	if (!fx) return;

	auto drawOne = [&](CStaticMeshRenderer* r, float scale, float yLift, const Color& c, BOOL tex) {
		if (!r) return;
		Vector3 p = center; p.y = ZFight::RangePanel + yLift;
		Matrix4x4 w = Matrix4x4::CreateScale(scale, 1.0f, scale) * Matrix4x4::CreateTranslation(p);
		fx->SetGPU();
		Renderer::SetBlendState(BS_ALPHABLEND);
		Renderer::DisableCulling(false);
		Renderer::SetDepthReadOnly();
		Renderer::SetWorldMatrix(&w);
		if (auto* mat = r->GetMaterial(0)) {
			MATERIAL old = mat->GetData(), tmp = old;
			tmp.Diffuse = c; tmp.TextureEnable = tex;
			mat->SetMaterial(tmp); r->Draw(); mat->SetMaterial(old);
		}
		Renderer::SetDepthEnable(true);
		Renderer::DisableCulling(true);
		Renderer::SetBlendState(BS_NONE);
		};

	// 塗り（action_ring は ±0.5 → 直径分の scale = radius*2、白αテクスチャ）
	drawOne(fill, radius * 2.0f, 0.0f,
		Color(color.x, color.y, color.z, ForecastUI::RingFillAlpha), TRUE);
	// ライン（ring_line は ±1 → scale = radius。DrawActionCircleLine と同じ 0.99 で塗りの縁の内側へ）
	drawOne(line, radius * 2.0f * 0.99f, 0.002f,
		Color(color.x, color.y, color.z, ForecastUI::RingBorderAlpha), FALSE);
}


// 放物線状の矢印：最前面オーバーレイを前提（深度／ブレンドはパス側に任せる）＋半透明
void Unit::DrawArcArrow(const Vector3& from, const Vector3& to, const Color& color) {
	CStaticMeshRenderer* arc = MeshManager::GetRenderer<CStaticMeshRenderer>("arrow_arc_mesh");
	CShader* fx = MeshManager::GetShader<CShader>("fxshader");
	if (!arc || !fx) return;

	Vector3 d = to - from; d.y = 0.0f;
	if (d.Length() < 0.05f) return;
	float rotY = -atan2f(d.z, d.x);
	Vector3 base = from; base.y = ZFight::Arrow;

	Matrix4x4 w = Matrix4x4::CreateRotationY(rotY) * Matrix4x4::CreateTranslation(base);
	fx->SetGPU();
	Renderer::DisableCulling(false);
	Renderer::SetWorldMatrix(&w);
	if (auto* mat = arc->GetMaterial(0)) {
		MATERIAL old = mat->GetData(), tmp = old;
		tmp.Diffuse = Color(color.x, color.y, color.z, color.w * ForecastUI::ArrowAlpha);
		tmp.TextureEnable = FALSE;
		mat->SetMaterial(tmp); arc->Draw(); mat->SetMaterial(old);
	}
	Renderer::DisableCulling(true);
}

void Unit::DrawWarningBox(const Vector3& center, float yaw, float size, const Color& color) {
	CStaticMeshRenderer* box = MeshManager::GetRenderer<CStaticMeshRenderer>("range_panel_mesh");
	CShader* fx = MeshManager::GetShader<CShader>("fxshader");
	if (!box || !fx) return;

	Vector3 p = center; p.y = ZFight::RangePanel;
	Matrix4x4 w = Matrix4x4::CreateScale(size, 1.0f, size)
		* Matrix4x4::CreateRotationY(yaw)
		* Matrix4x4::CreateTranslation(p);
	fx->SetGPU();
	Renderer::SetBlendState(BS_ALPHABLEND);
	Renderer::DisableCulling(false);
	Renderer::SetDepthReadOnly();
	Renderer::SetWorldMatrix(&w);
	if (auto* mat = box->GetMaterial(0)) {
		MATERIAL old = mat->GetData(), tmp = old;
		tmp.Diffuse = color; tmp.TextureEnable = FALSE;
		mat->SetMaterial(tmp); box->Draw(); mat->SetMaterial(old);
	}
	Renderer::SetDepthEnable(true);
	Renderer::DisableCulling(true);
	Renderer::SetBlendState(BS_NONE);
}

void Unit::DrawMoveCircle(const Vector3& center, float radius, const Color& fillColor, const Color& lineColor) {
	CStaticMeshRenderer* fill = MeshManager::GetRenderer<CStaticMeshRenderer>("action_ring_mesh");
	CStaticMeshRenderer* line = MeshManager::GetRenderer<CStaticMeshRenderer>("action_ring_line_mesh"); // ±1
	CShader* fx = MeshManager::GetShader<CShader>("fxshader");
	if (!fx) return;
	auto drawOne = [&](CStaticMeshRenderer* r, float scale, float yLift, const Color& c, BOOL tex) {
		if (!r) return;
		Vector3 p = center; p.y = ZFight::RangePanel + yLift;
		Matrix4x4 w = Matrix4x4::CreateScale(scale, 1.0f, scale) * Matrix4x4::CreateTranslation(p);
		fx->SetGPU();
		Renderer::SetBlendState(BS_ALPHABLEND);
		Renderer::DisableCulling(false);
		Renderer::SetDepthReadOnly();
		Renderer::SetWorldMatrix(&w);
		if (auto* mat = r->GetMaterial(0)) {
			MATERIAL old = mat->GetData(), tmp = old;
			tmp.Diffuse = c; tmp.TextureEnable = tex;
			mat->SetMaterial(tmp); r->Draw(); mat->SetMaterial(old);
		}
		Renderer::SetDepthEnable(true);
		Renderer::DisableCulling(true);
		Renderer::SetBlendState(BS_NONE);
		};
	drawOne(fill, radius * 2.0f, 0.0f, fillColor, TRUE);   // action_ring は ±0.5 → *2
	drawOne(line, radius * 0.99f, 0.002f, lineColor, FALSE);  // ring_line は ±1 → *0.99
}

void Unit::DrawMoveRangeCircle() {
	DrawMoveCircle(m_moveOrigin, m_moveBudget, s_moveFillColor, s_moveLineColor);
}

Vector3 Unit::ClampToMoveCircle(const Vector3& pos) const {
	Vector3 d = pos - m_moveOrigin; d.y = 0.0f;
	float dist = d.Length();
	if (dist > m_moveBudget && dist > 0.0001f) {
		d *= (m_moveBudget / dist);
		Vector3 r = m_moveOrigin + d; r.y = pos.y; return r;
	}
	return pos;
}

// 全ユニット共通の円 vs 円衝突（自分・死亡は除外）。接近対象も含めて押し出す
// → 敵は対象への接触距離（半径の合計）でピタッと停止
Vector3 Unit::ResolveUnitCollision(const Vector3& pos) const {
	Vector3 c = pos;
	auto pushOut = [&](Unit* u) {
		if (!u || u == this || u->GetHP() <= 0) return;
		Vector3 d = c - u->GetSRT().pos; d.y = 0.0f;
		float dist = d.Length();
		float minDist = m_bodyRadius + u->GetBodyRadius();
		if (dist < minDist && dist > 0.0001f) c += d * ((minDist - dist) / dist);
		};
	if (m_context) {
		pushOut(m_context->GetPlayer());
		pushOut(m_context->GetAlly());
		if (m_context->GetEnemyManager())
			for (Enemy* e : m_context->GetEnemyManager()->GetAllEnemies()) pushOut(e);
	}
	return c;
}
