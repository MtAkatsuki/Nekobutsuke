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
	const int ATTACK_RANGE = 1;            // 攻撃距離

	const float DEATH_FLY_DELAY = 0.2f;   // カメラの構図を整えた後、吹き飛ぶ前の間（攻撃アニメーションの代わりとなる観察時間）
	const float KILLCAM_HAZARD_DIST = 3.0f;   // 観察カメラとプレイヤーの水平方向の距離（1マス以上）
	const float KILLCAM_HAZARD_HEIGHT = 0.0f;   // 観察カメラの高さオフセット

	// 描画関連の定数
	const float UI_INPUT_COOLDOWN = 0.15f; // WASDの連続入力防止時間
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
	m_targetWorldPos = m_srt.pos;
	m_moveSpeed = MOVE_SPEED;

	SetFacing(Direction::South);
	m_srt.rot.y = m_targetRot.y;

	m_maxMovePoints = INITIAL_MOVE_POINTS;
	m_currentMovePoints = m_maxMovePoints;
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

	//アニメション終わった後、またメニュー遷移実際に実行する
	
	//メインメニュー状態の場合
	if (m_state == PlayerState::MENU_MAIN && m_nextState != PlayerState::MENU_MAIN) {
		if (m_nextState == PlayerState::MOVE_SELECT) SwitchToMoveSelect();
		else if (m_nextState == PlayerState::ATTACK_DIR_SELECT) SwitchToAttackDirSelect(AttackType::Push);
		else if (m_nextState == PlayerState::WAITING) EndTurn();
		m_nextState = m_state;
	}

	//プレーヤーの状態に応じた更新
	switch (m_state) {
	case PlayerState::FREE_MOVE:
		HandleFreeMove(deltaSeconds);
		break;

	case PlayerState::AIM:
		HandleAim(deltaSeconds);
		break;

	case PlayerState::MENU_MAIN:
		HandleMenuInput();
		break;

	case PlayerState::MOVE_SELECT:
		HandleMoveInput(deltaSeconds);
		if (m_state != PlayerState::MOVE_SELECT) break;
		m_srt.pos = GetMap()->GetWorldPosition(m_previewGridX, m_previewGridZ);
		UpdateWorldMatrix();
		// 移動予想位置に基づいて、プレイヤーの受けダメージ予測を計算
		CalculateMovePreviewDamage();
		break;

	case PlayerState::ANIM_MOVE:
		// 【追跡】：移動アニメーション中、プレイヤーを継続的に追従する
		if (m_context && m_context->GetCamera()) {
			m_context->GetCamera()->UpdateTrackingTarget(m_srt.pos);
		}
		if (UpdatePathMovement(deltaSeconds)) {
			m_hasMoved = true;
			// 現在位置のタイルのイベント（トラップ等）を発火させる
			Tile* finalTile = GetMap()->GetTile(m_gridX, m_gridZ);
			if (finalTile && finalTile->structure) {
				// プレイヤーがオブジェクトを踏んだ（進入した）際のイベントを実行
				finalTile->structure->OnEnter(this);
			}
			SwitchToMenuMain();
		}
		break;

	case PlayerState::ATTACK_DIR_SELECT:
		HandleAttackDirInput(deltaSeconds);
		m_srt.pos = GetMap()->GetWorldPosition(m_gridX, m_gridZ);
		// プレイヤーのダメージ予測を計算し、ターゲットのユニットへ設定
		{
			DirOffset offset = DirOffset::From(m_attackDir);
			Tile* targetTile = GetMap()->GetTile(m_gridX + offset.x, m_gridZ + offset.z);
			if (targetTile && this->CanTarget(targetTile->occupant)) {
				bool isPush = (m_selectedAttackType == AttackType::Push);
				int finalDmg = targetTile->occupant->CalculateExpectedDamage(m_playerDamage, isPush, m_attackDir);
				targetTile->occupant->SetPreviewDamage(finalDmg);
			}
		}
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
			if (m_isDebugAttack) {
				m_isDebugAttack = false;
				if (canControl) SwitchToMenuMain();
				else m_state = PlayerState::WAITING;
			}
			else {
				EndTurn();
			}
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
	// 【遮蔽】：移動先を選択中のステータスであれば、外部（敵）からの元の座標に対するダメージ注入を無視する。
	if (m_state == PlayerState::MOVE_SELECT) return;

	Unit::SetPreviewDamage(dmg);
}

void Player::StartTurn() {
	canControl = true;
	ResetMovePoints();
	m_hasMoved = false;
	m_isZoomedIn = false;

	m_startGridX = m_gridX;
	m_startGridZ = m_gridZ;
	m_previewGridX = m_gridX;
	m_previewGridZ = m_gridZ;

	m_moveStartPos = m_srt.pos;   // 行動円の中心をターン開始位置に固定

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

	canControl = false;
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
	if (m_context && GetMap()) {
		Tile* myTile = GetMap()->GetTile(m_gridX, m_gridZ);
		if (myTile && myTile->occupant == this) myTile->occupant = nullptr;
	}
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
	else if (state == TurnState::EnemyPhase) {
		canControl = false;
	}
}

void Player::OnDrawFloorUI(float /*deltaSeconds*/) {
	if (m_playerShader != nullptr) m_playerShader->SetGPU();

	// 三人称：行動範囲の発光リング（FREE_MOVE 中は常時表示）
	if (m_state == PlayerState::FREE_MOVE) {
		// 塗り環：暗めの半透明シアングリーン
		m_actionView->DrawActionCircle(m_moveStartPos, m_actionRadius, Color(0.15f, 0.75f, 0.55f, 0.6f));
		// ライン：明るいシアン
		m_actionView->DrawActionCircleLine(m_moveStartPos, m_actionRadius, Color(0.35f, 1.0f, 0.85f, 1.0f));
	}

	if (m_state == PlayerState::MOVE_SELECT) {
		m_actionView->DrawMoveRange(m_moveRangeTiles);
		m_actionView->DrawPathLine(m_currentPath, m_startGridX, m_startGridZ);
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
	else if (m_state == PlayerState::ATTACK_DIR_SELECT) {
		m_actionView->DrawAttackWarningFloor(m_gridX, m_gridZ, m_attackDir); // 赤い警告エリア（床面）
	}
}

void Player::OnDrawTransparent(float /*deltaSeconds*/) {
	if (m_state == PlayerState::MOVE_SELECT) {
		UpdateWorldMatrix(); 
		m_actionView->DrawGhost(m_renderer, m_srt.scale, m_srt.rot.y,
			m_startGridX, m_startGridZ, m_worldMatrix);
	}
}

void Player::OnDrawOverlay(float /*deltaSeconds*/) {
	if (m_state == PlayerState::ATTACK_DIR_SELECT) {
		// 攻撃プレビュー（敵のノックバック予測を含む最前面UI）を表示
		bool isPush = (m_selectedAttackType == AttackType::Push);
		m_actionView->DrawAttackWarningOverlay(m_gridX, m_gridZ, m_attackDir, isPush, this);
	}


	// 放物線状の矢印のみ最前面に表示（敵／罠に遮られない）。円は床レイヤー側で描画済み
	if (m_state == PlayerState::AIM && m_canAimHit && m_aimTarget) {
		DrawForecastArrow(m_aimTarget);
	}


}

void Player::SwitchToMenuMain() {
	m_state = PlayerState::MENU_MAIN;
	m_nextState = PlayerState::MENU_MAIN;

	// カメラ帰還
	if (m_context && m_context->GetCamera()) {
		m_context->GetCamera()->ChangeState(CameraState::Tracking, m_srt.pos);
	}

	if (m_hasMoved) m_context->GetUIManager()->SetMoveOptionEnabled(false);
	else m_context->GetUIManager()->SetMoveOptionEnabled(true);

	// 攻撃範囲内に敵がいるかどうかのチェック
	m_canAttack = false;
	int dx[] = { 0, 0, -1, 1 };
	int dz[] = { 1, -1, 0, 0 };
	for (int i = 0; i < 4; ++i) {
		for (int r = 1; r <= ATTACK_RANGE; ++r) {
			Tile* t = GetMap()->GetTile(m_gridX + dx[i] * r, m_gridZ + dz[i] * r);
			// マスに誰かが存在し、かつそれが自分自身でない場合のみ、押し出し（Push）操作を許可する
			if (t && t->occupant) {
				Unit* targetUnit = dynamic_cast<Unit*>(t->occupant);
				if (targetUnit && this->CanTarget(targetUnit)) {
					m_canAttack = true;
					break; // 対象が見つかった時点で、この方向のチェックを完了
				}
			}
			if (m_canAttack) break; // 敵が見つかっていれば全チェック終了
		}
	}

	m_context->GetUIManager()->SetAttackOptionEnabled(m_canAttack);
	m_context->GetUIManager()->HideGuideUI();
	m_context->GetUIManager()->OpenMainMenu();

	// プレーヤー位置の誤差修正
	m_srt.pos = GetMap()->GetWorldPosition(m_gridX, m_gridZ);
	UpdateWorldMatrix();
}

void Player::SwitchToMoveSelect() {
	m_state = PlayerState::MOVE_SELECT;
	m_context->GetUIManager()->CloseMenu();

	// 移動モード：矢印+ Enter + Esc
	m_context->GetUIManager()->ShowGuideUI(m_srt.pos, {
		.showArrows = true,
		.showEnter = true,
		.showEsc = true
		});

	m_previewGridX = m_gridX;
	m_previewGridZ = m_gridZ;

	m_moveRangeTiles = GetMap()->GetReachableTiles(m_gridX, m_gridZ, m_currentMovePoints);
	m_currentPath.clear();
}

void Player::SwitchToAttackDirSelect(AttackType type) {
	m_selectedAttackType = type;
	m_state = PlayerState::ATTACK_DIR_SELECT;
	m_context->GetUIManager()->CloseMenu();

	m_attackDir = m_facing;
	// 攻撃方向選択状態：矢印+ Enter + Esc
	m_context->GetUIManager()->ShowGuideUI(m_srt.pos, {
		.showArrows = true,
		.showEnter = true,
		.showEsc = true
		});
	// 【戦闘カメラ演出】：攻撃方向の選択時、カメラを攻撃方向へ少し前進（オフセット）させる
	if (m_context && m_context->GetCamera()) {
		DirOffset offset = DirOffset::From(m_attackDir);
		Vector3 warnCenter = GetMap()->GetWorldPosition(m_gridX + offset.x, m_gridZ + offset.z);
		m_context->GetCamera()->ChangeState(CameraState::ActionFocus, warnCenter);
	}
}

void Player::ExecuteMove() {
	if (m_currentPath.size() < 2) return;

	m_gridX = m_previewGridX;
	m_gridZ = m_previewGridZ;

	Tile* oldTile = GetMap()->GetTile(m_startGridX, m_startGridZ);
	if (oldTile) oldTile->occupant = nullptr;
	Tile* newTile = GetMap()->GetTile(m_gridX, m_gridZ);
	if (newTile) newTile->occupant = this;

	// プレイヤーの位置を予想ポイントからスタートポイントにリセット
	m_srt.pos = GetMap()->GetWorldPosition(m_startGridX, m_startGridZ);
	m_pathAnimIndex = 1; // １から始まる、０はスタート位置

	m_state = PlayerState::ANIM_MOVE;
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

void Player::HandleMenuInput() {
	// 移動していない場合のみ、Jキーで移動選択に切り替え
	if (!m_hasMoved && m_currentCmd.menuMove) {
		m_context->GetUIManager()->TriggerSelectAnim(0);
		m_nextState = PlayerState::MOVE_SELECT;
	}
	// 攻撃可能な場合のみKキーの入力を受け付ける
	else if (m_canAttack && m_currentCmd.menuAttack) {
		m_context->GetUIManager()->TriggerSelectAnim(1);
		m_nextState = PlayerState::ATTACK_DIR_SELECT;
	}
	else if (m_currentCmd.menuEnd) {
		m_context->GetUIManager()->TriggerSelectAnim(2);
		m_nextState = PlayerState::WAITING;
	}
}

void Player::HandleMoveInput(float dt) {
	// ESC: プレーヤー位置をリセットしてメインメニューに戻る
	if (m_currentCmd.cancel) {
		m_context->GetUIManager()->HideGuideUI();
		m_gridX = m_startGridX;
		m_gridZ = m_startGridZ;
		m_previewGridX = m_startGridX;
		m_previewGridZ = m_startGridZ;
		SwitchToMenuMain();
		return;
	}

	if (m_inputCooldown > 0.0f) m_inputCooldown -= dt;
	else {
		// スクリーン空間の入力を取得し、カメラの向きに応じてワールド格子方向へ変換
		DirOffset move = m_currentCmd.worldDir;
		if (move.x != 0 || move.z != 0) {

			int nextX = m_previewGridX + move.x;
			int nextZ = m_previewGridZ + move.z;

			// 移動できる範囲と予想移動先の検証
			bool inRange = false;
			for (auto* t : m_moveRangeTiles) {
				if (t->gridX == nextX && t->gridZ == nextZ) {
					inRange = true;
					break;
				}
			}

			if (inRange) {
				m_previewGridX = nextX;
				m_previewGridZ = nextZ;
				m_inputCooldown = UI_INPUT_COOLDOWN;
				// ルートの更新：startからpreviewまで
				std::vector<Tile*> path = GetMap()->FindPaths(m_startGridX, m_startGridZ, m_previewGridX, m_previewGridZ, true);
				m_currentPath.clear();

				// 最初にスタートタイルを追加
				Tile* startTile = GetMap()->GetTile(m_startGridX, m_startGridZ);
				if (startTile) m_currentPath.push_back(startTile);
				// 次に経由タイルを追加
				m_currentPath.insert(m_currentPath.end(), path.begin(), path.end());
				// 最後に目的地タイルを追加
				Tile* destTile = GetMap()->GetTile(m_previewGridX, m_previewGridZ);
				if (destTile) {
					m_currentPath.push_back(destTile);
				}

				SetFacingFromVector(Vector3((float)move.x, 0, (float)move.z));

				// 【カーソル移動】：カメラの目標注視点をカーソルのプレビュー位置に更新
				Vector3 previewPos = GetMap()->GetWorldPosition(m_previewGridX, m_previewGridZ);
				m_context->GetCamera()->UpdateTrackingTarget(previewPos);
			}
		}
	}

	if (m_currentCmd.submit) {
		m_context->GetUIManager()->HideGuideUI();
		ExecuteMove();
	}
}

void Player::HandleAttackDirInput(float dt) {
	if (m_currentCmd.cancel) {
		SwitchToMenuMain();
		return;
	}

	if (m_inputCooldown > 0.0f) m_inputCooldown -= dt;
	else {
		DirOffset move = m_currentCmd.worldDir;
		if (move.x != 0 || move.z != 0) {
			// 変換済みのワールド方向から攻撃方向と向きを設定
			m_attackDir = DirOffset::FromVector((float)move.x, (float)move.z);
			DirOffset offset = DirOffset::From(m_attackDir);
			SetFacingFromVector(Vector3((float)offset.x, 0, (float)offset.z));

			if (m_context && m_context->GetCamera()) {
				// 攻撃方向の変更に追従して、予警中心（対象マス）へ注視点を更新
				Vector3 warnCenter = GetMap()->GetWorldPosition(m_gridX + offset.x, m_gridZ + offset.z);
				m_context->GetCamera()->UpdateTrackingTarget(warnCenter);
			}
		}
	}

	if (m_currentCmd.submit) {
		m_context->GetUIManager()->HideGuideUI();
		ExecuteAttack();
	}
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
}

bool Player::DriveContinuous(const Vector3& worldDir, float dt) {
	if (worldDir.LengthSquared() < 0.0001f) return false;

	// ① 素の移動
	Vector3 newPos = m_srt.pos + worldDir * (m_moveSpeed * dt);
	// ② 壁との衝突解決（円 vs 近傍セル AABB の押し出し）
	newPos = GetMap()->ResolveCircleCollision(newPos, m_bodyRadius);
	// ③ 行動円クランプ（消費なし・毎回ここで丸める）
	newPos = ClampToActionCircle(newPos);

	m_srt.pos = newPos;

	// ④ 連続 yaw：進行方向へ滑らかに向く（atan2(x, z) は SetFacing と同じ規約）
	SetFacingYaw(atan2f(worldDir.x, worldDir.z));
	SyncGridFromWorld();
	return true;
}

Vector3 Player::ClampToActionCircle(const Vector3& pos) const {
	Vector3 d = pos - m_moveStartPos;
	d.y = 0.0f;
	float dist = d.Length();
	if (dist > m_actionRadius && dist > 0.0001f) {
		d *= (m_actionRadius / dist);           // 円周上へ投影
		Vector3 r = m_moveStartPos + d;
		r.y = pos.y;                            // 高さは元のまま
		return r;
	}
	return pos;
}

bool Player::UpdatePathMovement(float dt) {
	if (m_currentPath.empty()) return true; //パスは空なら終了

	Tile* targetTile = m_currentPath[m_pathAnimIndex];
	Vector3 targetPos = GetMap()->GetWorldPosition(targetTile->gridX, targetTile->gridZ);

	Vector3 diff = targetPos - m_srt.pos;
	if (diff.LengthSquared() > 0.001f) {
		SetFacingFromVector(diff);
	}

	// ベクトルの正規化と距離計算
	Vector3 dir = diff;
	float dist = dir.Length();
	float step = MOVE_SPEED * dt;

	if (dist <= step) {
		m_srt.pos = targetPos;
		++m_pathAnimIndex;// 次のターゲットへ
		// すべてのターゲットに到達したかチェック
		if (m_pathAnimIndex >= m_currentPath.size()) {
			return true; // 移動完了
		}
	}else {
		// まだ到達していない場合、進行
		dir.Normalize();
		m_srt.pos += dir * step;
	}

	UpdateWorldMatrix(); 
	return false; // 移動中
}

void Player::CalculateMovePreviewDamage() {
	int expectedDamage = 0;
	MapManager* map = GetMap();

	// 1. 予測：移動先が未発動の罠を踏んでしまうか
	if (Trap* trap = Trap::GetArmedTrap(map->GetTile(m_previewGridX, m_previewGridZ))) {
		expectedDamage += trap->GetTrapDamage();
	}

	// 2. 予測：敵の攻撃ロックオン範囲に侵入してしまうか
	if (m_context->GetEnemyManager()) {
		const auto& enemies = m_context->GetEnemyManager()->GetAllEnemies();
		for (auto* enemy : enemies) {
			// 敵がチャージ攻撃中で、かつプレイヤーの移動予定位置をロックオンしている場合
			if (enemy->IsCharging() &&
				enemy->GetLockedGridX() == m_previewGridX &&
				enemy->GetLockedGridZ() == m_previewGridZ)
			{
				// 押し出された後の連鎖衝突／罠ダメージを予測位置からシミュレート
				// （自分は移動済みの想定 → 占有判定から自身を除外）
				expectedDamage += enemy->GetEnemyDamage()
					+ SimulatePushChainDamage(map, m_previewGridX, m_previewGridZ,
						enemy->GetFacing(), m_onPushDamage, this);
			}
		}
	}

	m_previewDamage = expectedDamage;
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

void Player::DebugForceAttack(Direction dir, AttackType type) {
	m_attackDir = dir;
	m_selectedAttackType = type;
	m_isDebugAttack = true;
	SetFacing(dir);
	ExecuteAttack();
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