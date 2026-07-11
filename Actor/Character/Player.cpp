#include	"Player.h"	
#include    "PlayerActionView.h"
#include    "../../System/CDirectInput.h"
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
#include	<cmath>

namespace {
	// バランス・演出用の定数
	const int INITIAL_HP = 4;
	const int INITIAL_MOVE_POINTS = 4;
	const float MOVE_SPEED = 10.0f;        // タイル間移動速度
	const int ATTACK_RANGE = 1;            // 攻撃距離

	// 描画関連の定数
	const float UI_INPUT_COOLDOWN = 0.15f; // WASDの連続入力防止時間
	const float MODEL_SCALE = 0.8f;        // プレイヤーモデルの表示スケール

	// ジャンプ（祝賀）アニメーション
	const float JUMP_SPEED = 15.0f;
	const float JUMP_HEIGHT = 0.5f;
	const int MAX_JUMP_COUNT = 6;

	// 攻撃方向選択時のカメラ注視オフセット（グリッド単位）
	const float ATTACK_CAM_ENTER_OFFSET = 1.5f;  // 選択開始時の前進オフセット
	const float ATTACK_CAM_AIM_OFFSET = 2.5f;    // 方向変更時の前進オフセット

	// WASD / 矢印キーの方向入力を読む（スクリーン空間、上=+Z）。入力なしは {0,0}
	DirOffset ReadDirectionalInput() {
		auto& input = CDirectInput::GetInstance();
		if (input.CheckKeyBuffer(DIK_W) || input.CheckKeyBuffer(DIK_UP))    return { 0,  1 };
		if (input.CheckKeyBuffer(DIK_S) || input.CheckKeyBuffer(DIK_DOWN))  return { 0, -1 };
		if (input.CheckKeyBuffer(DIK_A) || input.CheckKeyBuffer(DIK_LEFT))  return { -1, 0 };
		if (input.CheckKeyBuffer(DIK_D) || input.CheckKeyBuffer(DIK_RIGHT)) return { 1,  0 };
		return { 0, 0 };
	}

	// スクリーン空間の入力を、カメラの向きインデックス(0~3)に応じて
	// ワールド格子方向へ回転する（DirOffset の回転代数を再利用）
	DirOffset RotateInputByCamera(int camDir, DirOffset in) {
		switch (camDir) {
		case 1:  return in.MoveLeft();   // 正面右カメラ
		case 2:  return in.MoveBack();   // 背面右カメラ
		case 3:  return in.MoveRight();  // 背面左カメラ
		default: return in;              // 正面左（基本カメラ）
		}
	}
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

	m_team = Team::Player;

	UpdateWorldMatrix();
}


void Player::Update(uint64_t dt) {
	Unit::Update(dt);
	float deltaSeconds = static_cast<float>(dt) / 1000.0f;
	if (m_context->GetUIManager()->IsAnimating()) {return;}

	// UIアニメーションが終わり、プレイヤーのUpdateが再開された最初のフレームでメニューを開く
	if (m_isWaitingTurnStart) {
		Ally* ally = m_context->GetAlly();
		// もし仲間が存在し、かつ脱出アニメーション（採掘やフェード）の最中であれば
		if (ally && ally->IsEscaping()) {
			// この間、プレイヤーの状態は WAITING のまま維持され、入力は受け付けず、カメラも仲間を注視し続ける
			return;
		}
		m_isWaitingTurnStart = false;
		SwitchToMenuMain();
	}

	// UIアニメーション終了後、最初のUpdateにてカメラのズームイン（接近）を開始
	if (!m_isZoomedIn) {
		m_isZoomedIn = true;
		if (m_context && m_context->GetCamera()) {
			m_context->GetCamera()->SetTargetRadius(Camera::ZOOM_RADIUS); 
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
	case PlayerState::MENU_MAIN:
		HandleMenuInput();
		break;

	case PlayerState::MOVE_SELECT:
		HandleMoveInput(deltaSeconds);
		if (m_state != PlayerState::MOVE_SELECT) break;
		m_srt.pos = m_context->GetMapManager()->GetWorldPosition(m_previewGridX, m_previewGridZ);
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
			Tile* finalTile = m_context->GetMapManager()->GetTile(m_gridX, m_gridZ);
			if (finalTile && finalTile->structure) {
				// プレイヤーがオブジェクトを踏んだ（進入した）際のイベントを実行
				finalTile->structure->OnEnter(this);
			}
			SwitchToMenuMain();
		}
		break;

	case PlayerState::ATTACK_DIR_SELECT:
		HandleAttackDirInput(deltaSeconds);
		m_srt.pos = m_context->GetMapManager()->GetWorldPosition(m_gridX, m_gridZ);
		// プレイヤーのダメージ予測を計算し、ターゲットのユニットへ設定
		{
			DirOffset offset = DirOffset::From(m_attackDir);
			Tile* targetTile = m_context->GetMapManager()->GetTile(m_gridX + offset.x, m_gridZ + offset.z);
			if (targetTile && targetTile->occupant && targetTile->occupant != this) {
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
		if (UpdateAttackAnimation(dt, nullptr)) {
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
		if (m_slideEndPos.LengthSquared() > 0.001f) {
			if (UpdateSlideAnimation(dt)) { 
				m_slideEndPos = Vector3(0, 0, 0);
				// 押し出された先のタイルにギミック（罠など）があるかチェック
				if (m_currentHP > 0) {
					Tile* t = m_context->GetMapManager()->GetTile(m_gridX, m_gridZ);
					if (t && t->structure) t->structure->OnEnter(this);
				}
				if (canControl && m_currentHP > 0) SwitchToMenuMain();
				else m_state = PlayerState::WAITING;
			}
		}
		else {
			// 壁に衝突して移動が発生しなかった場合
			if (canControl && m_currentHP > 0) SwitchToMenuMain();
			else m_state = PlayerState::WAITING;
		}
		break;

	case PlayerState::ANIM_CELEBRATE:
		UpdateCelebration(deltaSeconds);
		break;

	case PlayerState::WAITING:
		break;

	case PlayerState::DEAD_FLYING:
		UpdateDeathFly(deltaSeconds);
		break;
	}

	UpdateWorldMatrix();
}

void Player::OnDraw(uint64_t dt) {
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

void Player::OnPushed(Direction pushDir, Unit* attacker) {
	if (m_currentHP <= 0) return;
	m_state = PlayerState::KNOCKBACK;
	if (m_context && m_context->GetUIManager()) m_context->GetUIManager()->CloseMenu();
	Unit::OnPushed(pushDir);
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

	m_state = PlayerState::WAITING;
	m_isWaitingTurnStart = true;
}

void Player::EndTurn() {
	canControl = false;
	m_state = PlayerState::WAITING;
	m_context->GetUIManager()->CloseMenu();
	m_context->GetTurnManager()->RequestEndTurn();
}

void Player::TakeDamage(int damage, Unit* attacker) {
	Unit::TakeDamage(damage, attacker);
	if (m_currentHP <= 0 && m_state != PlayerState::DEAD_FLYING) {
		Die();
	}
}

void Player::Die() {
	m_state = PlayerState::DEAD_FLYING;
	if (m_context && m_context->GetMapManager()) {
		Tile* myTile = m_context->GetMapManager()->GetTile(m_gridX, m_gridZ);
		if (myTile && myTile->occupant == this) myTile->occupant = nullptr;
	}
	StartDeathFly();
	if (m_context && m_context->GetCamera())
		m_context->GetCamera()->PlayKillCam(m_hitSourcePos, m_srt.pos,true);
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

void Player::OnDrawFloorUI(uint64_t dt) {
	if (m_playerShader != nullptr) m_playerShader->SetGPU();

	if (m_state == PlayerState::MOVE_SELECT) {
		m_actionView->DrawMoveRange(m_moveRangeTiles);
		m_actionView->DrawPathLine(m_currentPath, m_startGridX, m_startGridZ);
	}
	else if (m_state == PlayerState::ATTACK_DIR_SELECT) {
		m_actionView->DrawAttackWarningFloor(m_gridX, m_gridZ, m_attackDir); // 赤い警告エリア（床面）
	}
}

void Player::OnDrawTransparent(uint64_t dt) {
	if (m_state == PlayerState::MOVE_SELECT) {
		UpdateWorldMatrix(); 
		m_actionView->DrawGhost(m_renderer, m_srt.scale, m_srt.rot.y,
			m_startGridX, m_startGridZ, m_worldMatrix);
	}
}

void Player::OnDrawOverlay(uint64_t dt) {
	if (m_state == PlayerState::ATTACK_DIR_SELECT) {
		// 攻撃プレビュー（敵のノックバック予測を含む最前面UI）を表示
		bool isPush = (m_selectedAttackType == AttackType::Push);
		m_actionView->DrawAttackWarningOverlay(m_gridX, m_gridZ, m_attackDir, isPush, this);
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
			Tile* t = m_context->GetMapManager()->GetTile(m_gridX + dx[i] * r, m_gridZ + dz[i] * r);
			// マスに誰かが存在し、かつそれが自分自身でない場合のみ、押し出し（Push）操作を許可する
			if (t && t->occupant) {
				Unit* targetUnit = dynamic_cast<Unit*>(t->occupant);
				if (targetUnit && targetUnit != this) {
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
	m_srt.pos = m_context->GetMapManager()->GetWorldPosition(m_gridX, m_gridZ);
	UpdateWorldMatrix();
}

void Player::SwitchToMoveSelect() {
	m_state = PlayerState::MOVE_SELECT;
	m_context->GetUIManager()->CloseMenu();

	// 移動モード：矢印+ Enter + Esc
	m_context->GetUIManager()->ShowGuideUI(
		m_srt.pos,  // 矢印を表示する位置
		true,       // Show Arrows
		true,       // Show Enter
		true        // Show Esc
	);

	m_previewGridX = m_gridX;
	m_previewGridZ = m_gridZ;

	m_moveRangeTiles = m_context->GetMapManager()->GetReachableTiles(m_gridX, m_gridZ, m_currentMovePoints);
	m_currentPath.clear();
}

void Player::SwitchToAttackDirSelect(AttackType type) {
	m_selectedAttackType = type;
	m_state = PlayerState::ATTACK_DIR_SELECT;
	m_context->GetUIManager()->CloseMenu();

	m_attackDir = m_facing;
	// 攻撃方向選択状態：矢印+ Enter + Esc
	m_context->GetUIManager()->ShowGuideUI(
		m_srt.pos,
		true,       // Show Arrows
		true,       // Show Enter
		true        // Show Esc
	);
	// 【戦闘カメラ演出】：攻撃方向の選択時、カメラを攻撃方向へ少し前進（オフセット）させる
	if (m_context && m_context->GetCamera()) {
		DirOffset offset = DirOffset::From(m_attackDir);
		// 攻撃方向へ 1.5 マス分オフセットさせた位置をターゲットにする
		Vector3 targetPos = m_srt.pos + Vector3((float)offset.x, 0.0f, (float)offset.z) * ATTACK_CAM_ENTER_OFFSET;
		m_context->GetCamera()->ChangeState(CameraState::ActionFocus, targetPos);
	}
}

void Player::ExecuteMove() {
	if (m_currentPath.size() < 2) return;

	m_gridX = m_previewGridX;
	m_gridZ = m_previewGridZ;

	Tile* oldTile = m_context->GetMapManager()->GetTile(m_startGridX, m_startGridZ);
	if (oldTile) oldTile->occupant = nullptr;
	Tile* newTile = m_context->GetMapManager()->GetTile(m_gridX, m_gridZ);
	if (newTile) newTile->occupant = this;

	// プレイヤーの位置を予想ポイントからスタートポイントにリセット
	m_srt.pos = m_context->GetMapManager()->GetWorldPosition(m_startGridX, m_startGridZ);
	m_pathAnimIndex = 1; // １から始まる、０はスタート位置

	m_state = PlayerState::ANIM_MOVE;
}

void Player::ExecuteAttack() {
	DirOffset offset = DirOffset::From(m_attackDir);
	int targetX = m_gridX + offset.x;
	int targetZ = m_gridZ + offset.z;
	Tile* targetTile = m_context->GetMapManager()->GetTile(targetX, targetZ);
	Vector3 targetPos = m_context->GetMapManager()->GetWorldPosition(targetX, targetZ);

	m_attackIsLethal = false;
	if (targetTile && targetTile->occupant && targetTile->occupant != this) {
		Unit* victim = targetTile->occupant;
		bool isPush = (m_selectedAttackType == AttackType::Push);
		int dmg = victim->CalculateExpectedDamage(m_playerDamage, isPush, m_attackDir);
		m_attackIsLethal = (victim->GetHP() - dmg <= 0);

		if (m_context && m_context->GetCamera()) {
			if (m_attackIsLethal)
				m_context->GetCamera()->PlayKillCam(m_srt.pos, victim->GetSRT().pos);
			else
				m_context->GetCamera()->PlayAttackZoom((m_srt.pos + targetPos) * 0.5f);
		}
	}
	else if (m_context && m_context->GetCamera()) {
		m_context->GetCamera()->PlayAttackZoom((m_srt.pos + targetPos) * 0.5f);
	}

	m_attackWindupTimer = 0.0f;
	m_state = PlayerState::ANIM_ATTACK_WINDUP;
}


void Player::PerformAttackStrike() {
	DirOffset offset = DirOffset::From(m_attackDir);
	int targetX = m_gridX + offset.x;
	int targetZ = m_gridZ + offset.z;

	Tile* targetTile = m_context->GetMapManager()->GetTile(targetX, targetZ);
	Vector3 targetPos = m_context->GetMapManager()->GetWorldPosition(targetX, targetZ);
	StartAttackAnimation(targetPos);

	if (targetTile && targetTile->occupant && targetTile->occupant != this) {
		Unit* victim = targetTile->occupant;
		if (m_selectedAttackType == AttackType::Push) {
			victim->OnPushed(m_attackDir, this);
		}
		victim->TakeDamage(m_playerDamage, this);
	}
	m_state = PlayerState::ANIM_ATTACK;
}

void Player::HandleMenuInput() {
	// 移動していない場合のみ、Jキーで移動選択に切り替え
	if (!m_hasMoved && CDirectInput::GetInstance().CheckKeyBufferTrigger(DIK_J)) {
		m_context->GetUIManager()->TriggerSelectAnim(0);
		m_nextState = PlayerState::MOVE_SELECT;
	}
	// 攻撃可能な場合のみKキーの入力を受け付ける
	else if (m_canAttack && CDirectInput::GetInstance().CheckKeyBufferTrigger(DIK_K)) {
		m_context->GetUIManager()->TriggerSelectAnim(1);
		m_nextState = PlayerState::ATTACK_DIR_SELECT;
	}
	else if (CDirectInput::GetInstance().CheckKeyBufferTrigger(DIK_L)) {
		m_context->GetUIManager()->TriggerSelectAnim(2);
		m_nextState = PlayerState::WAITING;
	}
}

void Player::HandleMoveInput(float dt) {
	// ESC: プレーヤー位置をリセットしてメインメニューに戻る
	if (CDirectInput::GetInstance().CheckKeyBufferTrigger(DIK_ESCAPE)) {
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
		DirOffset in = ReadDirectionalInput();
		if (in.x != 0 || in.z != 0) {
			int camDir = (m_context && m_context->GetCamera())
				? m_context->GetCamera()->GetNormalizedDirIndex() : 0;
			DirOffset move = RotateInputByCamera(camDir, in);

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
				std::vector<Tile*> path = m_context->GetMapManager()->FindPaths(m_startGridX, m_startGridZ, m_previewGridX, m_previewGridZ, true);
				m_currentPath.clear();

				// 最初にスタートタイルを追加
				Tile* startTile = m_context->GetMapManager()->GetTile(m_startGridX, m_startGridZ);
				if (startTile) m_currentPath.push_back(startTile);
				// 次に経由タイルを追加
				m_currentPath.insert(m_currentPath.end(), path.begin(), path.end());
				// 最後に目的地タイルを追加
				Tile* destTile = m_context->GetMapManager()->GetTile(m_previewGridX, m_previewGridZ);
				if (destTile) {
					m_currentPath.push_back(destTile);
				}

				SetFacingFromVector(Vector3((float)move.x, 0, (float)move.z));

				// 【カーソル移動】：カメラの目標注視点をカーソルのプレビュー位置に更新
				Vector3 previewPos = m_context->GetMapManager()->GetWorldPosition(m_previewGridX, m_previewGridZ);
				m_context->GetCamera()->UpdateTrackingTarget(previewPos);
			}
		}
	}

	if (CDirectInput::GetInstance().CheckKeyBufferTrigger(DIK_RETURN)) {
		m_context->GetUIManager()->HideGuideUI();
		ExecuteMove();
	}
}

void Player::HandleAttackDirInput(float dt) {
	if (CDirectInput::GetInstance().CheckKeyBufferTrigger(DIK_ESCAPE)) {
		SwitchToMenuMain();
		return;
	}

	if (m_inputCooldown > 0.0f) m_inputCooldown -= dt;
	else {
		DirOffset in = ReadDirectionalInput();
		if (in.x != 0 || in.z != 0) {
			int camDir = (m_context && m_context->GetCamera())
				? m_context->GetCamera()->GetNormalizedDirIndex() : 0;
			DirOffset move = RotateInputByCamera(camDir, in);

			// ワールド格子方向 → Direction 列挙（既存の FromVector を再利用）
			m_attackDir = DirOffset::FromVector((float)move.x, (float)move.z);

			// モデルの向きと戦術カメラのオフセットを攻撃方向に合わせて更新
			DirOffset offset = DirOffset::From(m_attackDir);
			SetFacingFromVector(Vector3((float)offset.x, 0, (float)offset.z));
			if (m_context && m_context->GetCamera()) {
				Vector3 targetPos = m_srt.pos + Vector3((float)offset.x, 0.0f, (float)offset.z) * ATTACK_CAM_AIM_OFFSET;
				m_context->GetCamera()->UpdateTrackingTarget(targetPos);
			}
		}
	}

	if (CDirectInput::GetInstance().CheckKeyBufferTrigger(DIK_RETURN)) {
		m_context->GetUIManager()->HideGuideUI();
		ExecuteAttack();
	}
}


bool Player::UpdatePathMovement(float dt) {
	if (m_currentPath.empty()) return true; //パスは空なら終了

	Tile* targetTile = m_currentPath[m_pathAnimIndex];
	Vector3 targetPos = m_context->GetMapManager()->GetWorldPosition(targetTile->gridX, targetTile->gridZ);

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
		m_pathAnimIndex++;// 次のターゲットへ
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
	MapManager* map = m_context->GetMapManager();

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

	Vector3 basePos = m_context->GetMapManager()->GetWorldPosition(m_gridX, m_gridZ);
	m_srt.pos.y = basePos.y + yOffset;

	// 着地の判定 (sin値が正から負に変わる瞬間を一回のジャンプ完了とみなす)
	if (previousSin >= 0.0f && currentSin < 0.0f) {
		m_jumpCount++;
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
}

void Player::DebugForceAttack(Direction dir, AttackType type) {
	m_attackDir = dir;
	m_selectedAttackType = type;
	m_isDebugAttack = true;
	SetFacing(dir);
	ExecuteAttack();
}