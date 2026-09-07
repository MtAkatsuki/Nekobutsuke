#include	"Player.h"	
#include    "PlayerActionView.h"
#include	"../../System/MeshManager.h"
#include	"../../GamePlay/Scene/GameScene.h"	
#include	"../../Core/GameContext.h"
#include	"../../GamePlay/Manager/MapManager.h"
#include	"../../UI/System/GameUIManager.h"
#include	"../../System//IScene.h"
#include    "../../System/CSprite.h"
#include	"../../System/ZFightTunables.h"
#include	"../../System/ModelRegistry.h"
#include	"../../GamePlay/Manager/EnemyManager.h"
#include	"../Character/Enemy.h"
#include	"../Gimmick/Trap.h"
#include	"../../Core/DebugLog.h"
#include "../../Actor/Character/Ally.h"
#include "../../System/Utility/WorldToScreen.h"
#include "../../Core/Application.h"
#include "../../System/ForecastTunables.h"
#include	<cmath>

namespace {
	// バランス・演出用の定数
	const int INITIAL_HP = 4;
	const int INITIAL_MOVE_POINTS = 4;
	const float MOVE_SPEED = 5.0f;        // タイル間移動速度

	const float DEATH_FLY_DELAY = 0.2f;   // カメラの構図を整えた後、吹き飛ぶ前の間（攻撃アニメーションの代わりとなる観察時間）
	const float KILLCAM_HAZARD_DIST = 3.0f;   // 観察カメラとプレイヤーの水平方向の距離（1マス以上）
	const float KILLCAM_HAZARD_HEIGHT = 0.0f;   // 観察カメラの高さオフセット

	// 描画関連の定数
	const float MODEL_SCALE = 0.7f;        // プレイヤーモデルの表示スケール

	// ジャンプ（祝賀）アニメーション
	const float JUMP_SPEED = 15.0f;
	const float JUMP_HEIGHT = 0.5f;
	const int MAX_JUMP_COUNT = 6;

	// 攻撃方向選択時のカメラ注視オフセット（グリッド単位）
	const float ATTACK_CAM_ENTER_OFFSET = 1.5f;  // 選択開始時の前進オフセット
	const float ATTACK_CAM_AIM_OFFSET = 2.5f;    // 方向変更時の前進オフセット
	// 攻撃予警区（玩家モデル外・正前方1ますの方形）
	const float AIM_WARN_SIZE = 1.0f;
	const float AIM_WARN_OFFSET = 1.0f;                          // 0.7→1.0：モデル外の正前方1格へ
	const Color AIM_WARN_COLOR = Color(1.0f, 0.2f, 0.2f, 0.25f);// 黄→薄い赤（攻撃予警）

	// 攻撃ロックHUD（2Dスクリーン）
	const int   AIM_ARROW_W = 67, AIM_ARROW_H = 79; 
	const int   AIM_CROSS_W = 44, AIM_CROSS_H = 44;
	const float AIM_HUD_Y = 1.0f;   // 敵の足元からHUDを出す高さ
	const float AIM_ARROW_GAP = 70.0f;  // 箭头と敵中心の基準距離(px)
	const float AIM_ARROW_BOB = 10.0f;  // 弾動の振幅(px)
	const float AIM_ARROW_SPEED = 6.0f;   // 弾動の速さ
	const float AIM_ARROW_SCALE = 1.0f;
	const float AIM_CROSS_SCALE = 1.0f;
	const Color AIM_HIT_COLOR = Color(0.2f, 1.0f, 0.35f, 1.0f);  // 命中可：緑
	const Color AIM_MISS_COLOR = Color(0.65f, 0.35f, 0.35f, 1.0f);// 命中不可：くすんだ赤
	const Color AIM_CROSS_COLOR = Color(1.0f, 0.2f, 0.2f, 1.0f);   // 赤X


}

//Spawnファクトリー
std::unique_ptr<Player> Player::Spawn(GameContext* ctx, int gridX, int gridZ, const Vector3& worldPos) {
	auto p = std::unique_ptr<Player>(new Player(ctx)); 
	p->Init(); 
	p->SetGridPosition(gridX, gridZ);
	p->SetPosition(worldPos);
	p->UpdateWorldMatrix();
	return p; 
}

Player::~Player() = default;

void Player::Init() {
	LoadPlayerResources();

	m_actionView = std::make_unique<PlayerActionView>();
	m_actionView->Init(m_context);

	m_srt.scale = Vector3(MODEL_SCALE, MODEL_SCALE, MODEL_SCALE);
	m_srt.rot = Vector3(0, 0, 0);

	m_state = PlayerState::WAITING;
	m_moveSpeed = MOVE_SPEED;

	SetFacing(Direction::South);
	m_srt.rot.y = m_targetRot.y;

	m_maxMovePoints = INITIAL_MOVE_POINTS;
	m_maxHP = INITIAL_HP;
	m_currentHP = m_maxHP;

	UpdateWorldMatrix();
}


void Player::Update(float deltaSeconds) {
	Unit::Update(deltaSeconds);
	if (m_context->GetUIManager()->IsAnimating()) {return;}

	// UIアニメーションが終わり、プレイヤーのUpdateが再開された最初のフレームでメニューを開く
	if (m_isWaitingTurnStart) {
		Ally* ally = m_context->GetAlly();
		// もし仲間が存在し、かつ脱出アニメーション（採掘やフェード）の最中であれば
		if (ally && ally->IsEscaping()) return;
		// この間、プレイヤーの状態は WAITING のまま維持され、入力は受け付けず、カメラも仲間を注視し続ける

		if (m_menuHold) return;   // カメラ帰還→UI再生中はメニューを開かない（急降下開始後に GameScene 側で解除）

		// 第三人称への急降下演出が完了するまで行動メニューを開かない
		if (Camera* cam = m_context ? m_context->GetCamera() : nullptr) {
			if (cam->IsActorTransitioning()) return;
			if (cam->GetViewMode() == ViewMode::Battle && !cam->IsAtTarget()) return;
		}
		m_isWaitingTurnStart = false;
		m_state = PlayerState::FREE_MOVE;   // メニューを廃し、直接ドライブ状態へ
	}

	// UIアニメーション終了後、最初のUpdateにてカメラのズームイン（接近）を開始
	if (!m_isZoomedIn) {
		m_isZoomedIn = true;
		if (m_context && m_context->GetCamera()) {
			m_context->GetCamera()->SetTargetRadius(m_context->GetCamera()->GetTrackingRadius());
		}
	}

	UpdateFacingRotation(deltaSeconds);

	//プレーヤーの状態に応じた更新
	switch (m_state) {
	case PlayerState::FREE_MOVE:
		HandleFreeMove(deltaSeconds);
		break;

	case PlayerState::AIM:
		HandleAim(deltaSeconds);
		break;

	case PlayerState::ANIM_ATTACK_WINDUP: {
		m_attackWindupTimer += deltaSeconds;
		float lead = m_attackIsLethal ? Camera::KILLCAM_LEAD : Camera::ATTACK_ZOOM_LEAD;
		if (m_attackWindupTimer >= lead) {
			PerformAttackStrike();
		}
		break;
	}

	case PlayerState::ANIM_ATTACK:
		if (UpdateAttackAnimation(deltaSeconds, nullptr)) {
			EndTurn();
		}
		break;

	case PlayerState::KNOCKBACK:
		break;   // 击退は Unit::UpdateKnockback が担当

	case PlayerState::ANIM_CELEBRATE:
		UpdateCelebration(deltaSeconds);
		break;

	case PlayerState::WAITING:
		break;

	case PlayerState::DEAD_FLYING:
		if (m_deathFlyDelay > 0.0f) { m_deathFlyDelay -= deltaSeconds; break; }  // 停顿中は飛ばさない
		UpdateDeathFly(deltaSeconds);
		break;
	}
	UpdateWorldMatrix();
}

void Player::OnDraw(float /*deltaSeconds*/) {
	if (m_playerShader != nullptr) m_playerShader->SetGPU();
	DrawModel();
}

void Player::StartCelebration() {
	m_state = PlayerState::ANIM_CELEBRATE;
	m_jumpCount = 0;
	m_jumpTimer = 0.0f;
	m_isCelebrationDone = false;
	m_context->GetUIManager()->CloseMenu();
	SetFacing(Direction::South);
}

void Player::OnKnockbackBegin() {
	m_state = PlayerState::KNOCKBACK;
}
void Player::OnKnockbackEnd() {
	if (m_currentHP <= 0) return;      // 死亡は TakeDamage→Die が処理済み
	m_state = PlayerState::WAITING;    // 敵ターン限定なので常に待機へ戻る
}

void Player::SetPreviewDamage(int dmg) {
	Unit::SetPreviewDamage(dmg);
}

void Player::StartTurn() {
	m_isZoomedIn = false;

	m_moveOrigin = m_srt.pos;
	m_moveBudget = m_actionRadius;

	m_state = PlayerState::WAITING;
	m_isWaitingTurnStart = true;
}

void Player::EndTurn() {
	// 足元の罠を発火（止まった場所で踏む）
	if (Tile* t = GetMap()->GetTile(m_gridX, m_gridZ)) {
		if (t->structure) t->structure->OnEnter(this);
	}
	// 罠で死んだら死亡演出に任せ、通常のターン終了処理は行わない（state を上書きしない）
	if (m_state == PlayerState::DEAD_FLYING) return;

	m_state = PlayerState::WAITING;
	m_context->GetUIManager()->CloseMenu();
	GetTurnManager()->RequestEndTurn();
}

void Player::TakeDamage(int damage, Unit* attacker) {
	Unit::TakeDamage(damage, attacker);
	if (m_currentHP <= 0 && m_state != PlayerState::DEAD_FLYING) {
		m_killedByHazard = (attacker == nullptr);   // 攻撃者なし＝罠など
		Die();
	}
}

void Player::Die() {
	m_state = PlayerState::DEAD_FLYING;

	Camera* cam = m_context ? m_context->GetCamera() : nullptr;

	if (m_killedByHazard) {
		Vector3 camPos = cam ? cam->GetPosition() : (m_srt.pos - Vector3(0, 0, 3));
		Vector3 away = m_srt.pos - camPos;
		away.y = 0.0f;
		if (away.LengthSquared() > 0.01f) away.Normalize();
		else away = Vector3(0, 0, 1);

		m_hitSourcePos = m_srt.pos + away;
		StartDeathFly();
		m_deathFlyDelay = DEATH_FLY_DELAY;      // ① 構図を整えた後、まず少し間を置いてから吹き飛ばす

		if (cam) {
			// ② カメラをプレイヤーの後方へ引き上げ、観察用の構図を作る
			Vector3 obsPos = m_srt.pos - away * KILLCAM_HAZARD_DIST
				+ Vector3(0, KILLCAM_HAZARD_HEIGHT, 0);
			cam->SetPosition(obsPos);

			Vector3 observer = m_srt.pos + away * 2.0f;
			cam->PlayKillCam(observer, m_srt.pos, true);
		}
	}
	else {
		StartDeathFly();
		m_deathFlyDelay = 0.0f;                 // 通常撃破は攻撃アニメーションがあるため、追加の待機は不要
		if (cam) cam->PlayKillCam(m_hitSourcePos, m_srt.pos, true);
	}
}

void Player::OnDeathFlyComplete() {
	Destroy();
}

void Player::OnTurnChanged(TurnState state) {
	if (state == TurnState::PlayerPhase) {
		StartTurn();
	}
}

void Player::OnDrawFloorUI(float /*deltaSeconds*/) {
	if (m_playerShader != nullptr) m_playerShader->SetGPU();

	// 三人称：行動範囲の発光リング（FREE_MOVE 中は常時表示）
	if (m_state == PlayerState::FREE_MOVE) {
		DrawMoveRangeCircle();
	}

	if (m_state == PlayerState::AIM) {
		// プレイヤーの攻撃予警範囲（矩形）
		Vector3 center; float yaw;
		GetAimBox(center, yaw);
		m_actionView->DrawAimWarningBox(center, yaw, AIM_WARN_SIZE, AIM_WARN_COLOR);

		// 敵の被弾円（照準中は常に）＋着地点の円（命中時）：床デカール＝敵／罠が上に乗る
		if (m_aimTarget) {
			DrawHitRing(m_aimTarget);
			if (m_canAimHit) DrawLandingRing(m_aimTarget);
		}
	}
}

void Player::OnDrawTransparent(float /*deltaSeconds*/) {
}

void Player::OnDrawOverlay(float /*deltaSeconds*/) {
	// 放物線状の矢印のみ最前面に表示（敵／罠に遮られない）。円は床レイヤー側で描画済み
	if (m_state == PlayerState::AIM && m_canAimHit && m_aimTarget) {
		DrawForecastArrow(m_aimTarget);
	}
}

void Player::ExecuteAttack() {
	if (!m_aimTarget) return;
	Vector3 vpos = m_aimTarget->GetSRT().pos;
	m_attackPushDir = vpos - m_srt.pos;   // 玩家→敵（押し出す向き）
	m_attackPushDir.y = 0.0f;

	// 致死判定（KillCam / 通常ズームの分岐用）
	int dmg = m_aimTarget->CalculateExpectedDamage(m_playerDamage, true, m_attackPushDir);
	m_attackIsLethal = (m_aimTarget->GetHP() - dmg <= 0);

	if (Camera* cam = m_context ? m_context->GetCamera() : nullptr) {
		if (m_attackIsLethal) cam->PlayKillCam(m_srt.pos, vpos);
		else                  cam->PlayAttackZoom((m_srt.pos + vpos) * 0.5f);
	}
	m_attackWindupTimer = 0.0f;
	m_state = PlayerState::ANIM_ATTACK_WINDUP;
}


void Player::PerformAttackStrike() {
	if (!m_aimTarget) { m_state = PlayerState::ANIM_ATTACK; return; }
	Vector3 vpos = m_aimTarget->GetSRT().pos;

	StartAttackAnimation(vpos);
	m_aimTarget->OnPushed(m_attackPushDir, this);   // 連続ノックバック
	m_aimTarget->TakeDamage(m_playerDamage, this);
	m_state = PlayerState::ANIM_ATTACK;
}

void Player::HandleFreeMove(float dt) {
	// ESC：ターン終了
	if (m_currentCmd.endTurn) { EndTurn(); return; }

	// 右クリック：攻撃モードへ（攻撃大ブロックで実装。今は入口のみ）
	if (m_currentCmd.aimToggle) { EnterAim(); return; }

	// WASD 連続ドライブ
	Vector3 dir(m_currentCmd.moveX, 0.0f, m_currentCmd.moveZ);
	if (DriveContinuous(dir, dt)) {
		// カメラ追従：注視点を自機へ
		if (m_context && m_context->GetCamera())
			m_context->GetCamera()->UpdateTrackingTarget(m_srt.pos);
	}
	UpdateWorldMatrix();
	UpdateTrapPreview();
}

bool Player::DriveContinuous(const Vector3& worldDir, float dt) {
	if (worldDir.LengthSquared() < 0.0001f) return false;

	// ① 素の移動
	Vector3 newPos = m_srt.pos + worldDir * (m_moveSpeed * dt);
	// ② 壁との衝突解決（円 vs 近傍セル AABB の押し出し）
	newPos = GetMap()->ResolveCircleCollision(newPos, m_bodyRadius);
	// ③ 行動円クランプ（消費なし・毎回ここで丸める）
	newPos = ResolveUnitCollision(newPos);   // 他ユニット回避（統一）
	newPos = ClampToMoveCircle(newPos); 

	m_srt.pos = newPos;

	// ④ 連続 yaw：進行方向へ滑らかに向く（atan2(x, z) は SetFacing と同じ規約）
	SetFacingYaw(atan2f(worldDir.x, worldDir.z));
	SyncGridFromWorld();
	return true;
}


void Player::UpdateCelebration(float dt) {
	// 前回のサイン値と今回のサイン値を比較して、ジャンプの上下動を実現
	float previousSin = sinf(m_jumpTimer * JUMP_SPEED);
	m_jumpTimer += dt;
	float currentSin = sinf(m_jumpTimer * JUMP_SPEED);
	//fabsfを使って、サイン波の上下動を正の値に変換し、ジャンプの高さを調整
	float yOffset = fabsf(currentSin) * JUMP_HEIGHT;

	Vector3 basePos = GetMap()->GetWorldPosition(m_gridX, m_gridZ);
	m_srt.pos.y = basePos.y + yOffset;

	// 着地の判定 (sin値が正から負に変わる瞬間を一回のジャンプ完了とみなす)
	if (previousSin >= 0.0f && currentSin < 0.0f) {
		++m_jumpCount;
	}

	if (m_jumpCount >= MAX_JUMP_COUNT) {
		m_srt.pos.y = basePos.y; // 確実に接地させる
		m_isCelebrationDone = true;
	}
}

void Player::LoadPlayerResources() {
	auto* renderer = ModelRegistry::RegisterModel(
		"player_mesh", "Assets/model/character/Mouse/Mouse_01.obj", "Assets/model/character/Mouse");
	SetModelRenderer(renderer);

	m_playerShader = MeshManager::GetShader<CShader>("toonshader");
	if (!m_playerShader) {
		DBG_ERROR("CRITICAL ERROR: 'toonshader' not found! Check shader file paths.");
	}

	m_aimArrowSprite = std::make_unique<CSprite>(AIM_ARROW_W, AIM_ARROW_H, "Assets/texture/UI/ui_aim_arrow.png");
	m_aimCrossSprite = std::make_unique<CSprite>(AIM_CROSS_W, AIM_CROSS_H, "Assets/texture/UI/ui_aim_cross.png");
}

void Player::DebugForceAttack() {
	SelectNearestEnemy();                 // 前方の敵をロック
	if (m_aimTarget) ExecuteAttack();     // 新AIM攻撃で発動（KillCam/AttackZoomも実行）
}


void Player::SelectNearestEnemy() {
	m_aimTarget = nullptr;

	float best = 3.4e38f;   // 十分大きな初期値
	auto consider = [&](Unit* u) {
		if (!u || !CanTarget(u)) return;
		Vector3 d = u->GetSRT().pos - m_srt.pos; d.y = 0.0f;
		float dist = d.LengthSquared();
		if (dist < best) { best = dist; m_aimTarget = u; }
		};
	if (auto* em = m_context->GetEnemyManager())
		for (Enemy* e : em->GetAllEnemies()) consider(e);
	consider(m_context->GetAlly());   // 味方も候補に
}

void Player::CycleTarget(int step) {
	std::vector<Unit*> cand;
	if (auto* em = m_context->GetEnemyManager())
		for (Enemy* e : em->GetAllEnemies()) if (CanTarget(e)) cand.push_back(e);
	if (Unit* a = m_context->GetAlly()) if (CanTarget(a)) cand.push_back(a);
	if (cand.empty()) { m_aimTarget = nullptr; return; }
	int idx = 0;
	for (int i = 0; i < (int)cand.size(); ++i) if (cand[i] == m_aimTarget) { idx = i; break; }
	idx = (idx + step + (int)cand.size()) % (int)cand.size();
	m_aimTarget = cand[idx];
}

void Player::EnterAim() {
	SelectNearestEnemy();
	if (!m_aimTarget) return;                 // 攻撃対象がいなければ AIM に入らない

	m_state = PlayerState::AIM;
	if (Camera* cam = m_context ? m_context->GetCamera() : nullptr) {
		cam->BeginAimFollow();                                          // オフセット初期化
		cam->AimFollow(m_srt.pos, m_aimTarget->GetSRT().pos, 0.0f, 0.0f, 0.0f);  // 初回構図
	}
}

void Player::ExitAim() {
	m_aimTarget = nullptr;
	m_state = PlayerState::FREE_MOVE;
	if (Camera* cam = m_context ? m_context->GetCamera() : nullptr) {
		cam->ChangeState(CameraState::Tracking, m_srt.pos);  // 通常追従へ
		cam->OrientBehind(m_srt.rot.y);                      // 玩家の背後へ戻す
	}
}

void Player::HandleAim(float dt) {
	// 右クリックで退出
	if (m_currentCmd.aimToggle) { ExitAim(); return; }

	// QE で対象切替
	if (m_currentCmd.targetPrev) CycleTarget(-1);
	if (m_currentCmd.targetNext) CycleTarget(+1);

	// 対象が死亡/消滅したら選び直し、いなければ退出
	if (!m_aimTarget || !CanTarget(m_aimTarget)) {
		SelectNearestEnemy();
		if (!m_aimTarget) { ExitAim(); return; }
	}

	// 攻撃モードでも自由移動（行動円内クランプ＋壁衝突は DriveContinuous が担当）
	Vector3 dir(m_currentCmd.moveX, 0.0f, m_currentCmd.moveZ);
	DriveContinuous(dir, dt);

	// 攻撃モード：目標敵の身体に食い込ませない（円-円で最小距離を保つ）。
	// これで玩→敵方向が近距離で急変せず、肩ワープを根絶する。
	if (m_aimTarget) {
		Vector3 ep = m_aimTarget->GetSRT().pos;
		Vector3 d = m_srt.pos - ep; d.y = 0.0f;
		float minDist = m_bodyRadius + m_aimTarget->GetBodyRadius();
		float dist = d.Length();
		if (dist < minDist && dist > 0.0001f) {
			Vector3 pushed = ep + (d / dist) * minDist;   // 最小距離の円周へ押し出す
			m_srt.pos.x = pushed.x;
			m_srt.pos.z = pushed.z;
			UpdateWorldMatrix();
		}
	}

	// ただし向きは常に敵へ（DriveContinuous の進行方向 yaw を上書き）
	Vector3 toE = m_aimTarget->GetSRT().pos - m_srt.pos;
	if (toE.LengthSquared() > 0.0001f) SetFacingYaw(atan2f(toE.x, toE.z));

	m_canAimHit = IsAimHit();   // 毎フレーム命中可否を更新（矢印色・確定に使う）

	// 命中中：敵HPバーに予測ダメージを出す（点滅プレビュー）
	if (m_canAimHit && m_aimTarget) {
		Vector3 pd = m_aimTarget->GetSRT().pos - m_srt.pos; pd.y = 0.0f;
		m_aimTarget->SetPreviewDamage(
			m_aimTarget->CalculateExpectedDamage(m_playerDamage, true, pd));
	}
	// 左クリック＋命中で攻撃確定
	if (m_currentCmd.attackConfirm && m_canAimHit) {
		ExecuteAttack();
		return;
	}

	// 毎フレーム、右肩越しに敵を捉える構図へ
	if (Camera* cam = m_context ? m_context->GetCamera() : nullptr)
		cam->AimFollow(m_srt.pos, m_aimTarget->GetSRT().pos,
			m_currentCmd.aimYawDelta, m_currentCmd.aimPitchDelta, dt);

	// 左クリック確定は「判定円 ∩ 受击盒」実装後（小步4）で接続
	UpdateWorldMatrix();
	UpdateTrapPreview();
	m_aimArrowAnimTimer += dt;
}

void Player::GetAimBox(Vector3& center, float& yaw) const {
	yaw = m_srt.rot.y;                              // AIM 中は敵を向いている
	Vector3 fwd(sinf(yaw), 0.0f, cosf(yaw));        // yaw の前方（SetFacing と同じ規約）
	center = m_srt.pos + fwd * AIM_WARN_OFFSET;
}

bool Player::IsAimHit() const {
	if (!m_aimTarget) return false;
	using namespace GM31::GE::Collision;

	Vector3 center; float yaw;
	GetAimBox(center, yaw);

	// 予警区を OBB（yaw 回転・1x1・y は判定を平面化するため厚め）で表現
	BoundingBoxOBB obb = SetOBB(Vector3(0.0f, yaw, 0.0f), center,
		AIM_WARN_SIZE, 2.0f, AIM_WARN_SIZE);
	// 敵の受击円
	BoundingSphere sph;
	sph.center = m_aimTarget->GetSRT().pos;
	sph.radius = ForecastUI::HitRingRadius;

	return CollisionSphereOBB(sph, obb);
}

void Player::DrawUI() {
	Unit::DrawUI();   // 玩家自身のHPバー

	if (m_state == PlayerState::AIM && m_aimTarget) {
		DrawAimHUD();
	}
}

void Player::DrawAimHUD() {
	Camera* cam = m_context ? m_context->GetCamera() : nullptr;
	if (!cam || !m_aimArrowSprite || !m_aimCrossSprite) return;

	float sw = (float)Application::GetWidth();
	float sh = (float)Application::GetHeight();

	Vector3 ep = m_aimTarget->GetSRT().pos;
	ep.y += AIM_HUD_Y;
	Vector2 sp = WorldToScreen(ep, cam->GetViewMatrix(), cam->GetProjMatrix(), sw, sh);

	// --- 両脇の箭头 ---
	Color arrowCol = m_canAimHit ? Color(1.0f, 1.0f, 1.0f, 1.0f)
		: Color(0.45f, 0.45f, 0.45f, 1.0f);
	MATERIAL am{};
	am.Diffuse = arrowCol;
	am.TextureEnable = TRUE;
	m_aimArrowSprite->ModifyMtrl(am);

	float bob = sinf(m_aimArrowAnimTimer * AIM_ARROW_SPEED);
	float off = AIM_ARROW_GAP + bob * AIM_ARROW_BOB;
	Vector3 scale(AIM_ARROW_SCALE, AIM_ARROW_SCALE, 1.0f);

	// 左（敵の左・右向きで敵を指す＝原画のまま）／右（敵の右・180°回転で左向き）
	m_aimArrowSprite->Draw(scale, Vector3(0, 0, -PI/2), Vector3(sp.x - off, sp.y, 0.0f));
	m_aimArrowSprite->Draw(scale, Vector3(0, 0, PI/2), Vector3(sp.x + off, sp.y, 0.0f));

	// 攻撃不可：左右の箭头それぞれの「運動中心」に赤Xを重ねる（Xは静止・アニメなし）
	if (!m_canAimHit) {
		Vector3 cs(AIM_CROSS_SCALE, AIM_CROSS_SCALE, 1.0f);
		m_aimCrossSprite->Draw(cs, Vector3(0, 0, 0), Vector3(sp.x - AIM_ARROW_GAP, sp.y, 0.0f));
		m_aimCrossSprite->Draw(cs, Vector3(0, 0, 0), Vector3(sp.x + AIM_ARROW_GAP, sp.y, 0.0f));
	}
}

void Player::SyncGridFromWorld() {
	int gx, gz;
	if (GetMap()->WorldToGrid(m_srt.pos, gx, gz)) {
		m_gridX = gx;
		m_gridZ = gz;   // 罠・占有・攻撃判定の基準となる論理格を追随させるだけ
	}
}

void Player::UpdateTrapPreview() {
	if (!GetMap()) return;
	if (Trap* trap = Trap::GetArmedTrap(GetMap()->GetTile(m_gridX, m_gridZ)))
		m_previewDamage = trap->GetTrapDamage();
}