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


namespace {
	// 演出・バランス用定数
	const int INITIAL_HP = 5;
	const int INITIAL_MOVE_POINTS = 4;
	const float MOVE_SPEED = 5.0f;
	const float CHARGE_SHAKE_AMP = 0.01f;      // 蓄力時の震え幅
	const float ATTACK_DELAY = 0.1f;           // 攻撃開始前のディレイ
	const int ATTACK_RANGE = 1;				   // 攻撃範囲（グリッド単位）
	const float MODEL_SCALE = 0.8f;            // 敵モデルの表示スケール

	const int DIST_UNREACHABLE = 999;          // ターゲット不在時の距離初期値（十分大きな値）
	const float ARRIVE_EPSILON_SQ = 0.01f;     // タイル到着判定の距離二乗しきい値
	const float AXIS_BIAS_THRESHOLD = 0.1f;    // 方向判定でX/Z軸を区別する最小差分

	// 攻撃意図矢印（オーバーレイ）の表示パラメータ
	const float ATTACK_ARROW_POS_RATIO = 0.35f;                       // 自分→対象間の矢印配置比率
	const float ATTACK_ARROW_SCALE = 0.6f;                            // 矢印の表示スケール
	const Color ATTACK_ARROW_COLOR = Color(1.0f, 0.08f, 0.57f, 0.7f); // 赤ピンク色で危険を通知

	const float MISS_NUM_Y_OFFSET = 1.0f;      // 空振り（miss）数字の表示高さ

	// --- 床ヒントUIの表示色 ---
	const Color MOVE_RANGE_COLOR = Color(0.0f, 1.0f, 0.0f, 0.2f);   // 移動範囲：薄い緑
	const Color DANGER_TILE_COLOR = Color(0.9f, 0.0f, 0.0f, 0.5f);  // ロックオン中のタイル：半透明の赤
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
	m_targetWorldPos = m_srt.pos;
	m_moveSpeed = MOVE_SPEED;

	m_maxMovePoints = INITIAL_MOVE_POINTS;
	m_currentMovePoints = m_maxMovePoints;
	m_maxHP = INITIAL_HP;
	m_currentHP = m_maxHP;
	m_state = EnemyState::IDLE;
	m_isDead = false;

	m_pushArrowRenderer = MeshManager::GetRenderer<CStaticMeshRenderer>("arrow_push_mesh");
	m_attackArrowRenderer = MeshManager::GetRenderer<CStaticMeshRenderer>("arrow_attack_mesh");
	if (!m_attackArrowRenderer) m_attackArrowRenderer = m_pushArrowRenderer;

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

	// 敵のダメージ予測を計算し、ターゲットのユニットへ設定
	if (m_isCharging && !m_isDead && m_state != EnemyState::DEAD_FLYING) {
		Tile* lockedTile = GetMap()->GetTile(m_lockedGridX, m_lockedGridZ);
		if (lockedTile && lockedTile->occupant && lockedTile->occupant != this) {
			int finalDmg = lockedTile->occupant->CalculateExpectedDamage(m_enemyDamage, true, m_facing);
			lockedTile->occupant->SetPreviewDamage(finalDmg);
		}
	}

	switch (m_state) {
	case EnemyState::MOVING:
		UpdateMove(deltaSeconds);
		break;

	case EnemyState::ATTACKING:

		m_attackTimer += deltaSeconds;
		if (m_attackTimer > ATTACK_DELAY) {
			auto impactCallback = [this]() {
				Tile* targetTile = GetMap()->GetTile(m_lockedGridX, m_lockedGridZ);
				if (targetTile != nullptr && this->CanTarget(targetTile->occupant)) {
					// 移動後に参照が失われないよう、事前に対象オブジェクトのポインターを保存
					Unit* victim = targetTile->occupant;

					// 1. 先に押し出し（ノックバック）の物理効果を発生させる
					victim->OnPushed(this->m_facing,this);

					// 2. その後、ダメージ処理を実行（m_enemyDamage を実際のダメージ変数や数値に置き換え）
					victim->TakeDamage(m_enemyDamage, this);
				}
				else {//miss
					if (m_context->GetDamageManager()) {
						Vector3 missPos = GetMap()->GetWorldPosition(m_lockedGridX, m_lockedGridZ);
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
		if (m_slideEndPos.LengthSquared() > 0.001f) {
			if (UpdateSlideAnimation(deltaSeconds)) {
				m_state = EnemyState::IDLE;
				m_slideEndPos = Vector3(0, 0, 0);
				if (m_currentHP <= 0) {
					// 死亡して吹き飛ぶ演出へ移行。罠のトリガーロジックは完全にスキップ！
					Die();
				}
				else {
					m_state = EnemyState::IDLE;
					// 生存している場合のみ、タイルのイベント（罠など）を発生させる
					Tile* currentTile = GetMap()->GetTile(m_gridX, m_gridZ);
					if (currentTile && currentTile->structure) {
						currentTile->structure->OnEnter(this);
					}
				}
			}
		}
		else {
			if (UpdateAttackAnimation(deltaSeconds, nullptr)) {
				// 壁に激突した後も生死判定を行い、HPが0なら死亡処理を実行する
				if (m_currentHP <= 0) Die();
				else m_state = EnemyState::IDLE;// 生き残っている場合のみIDLEへ
			}
		}
		break;
	default:break;
	}

	UpdateWorldMatrix();
}

void Enemy::OnDraw(float /*deltaSeconds*/) {

	if (m_enemyShader != nullptr) m_enemyShader->SetGPU();

	// 蓄力中の震えは `m_srt.pos` を直接汚染せず、描画用の一時的な Matrix で処理する
	if (m_isCharging) {
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
		ExecuteAI();
	}
}

void Enemy::OnTurnChanged(TurnState state) {
	if (state == TurnState::EnemyPhase) StartTurn();
}

void Enemy::OnPushed(Direction pushDir, Unit* attacker) {
	if (m_currentHP <= 0 || m_state == EnemyState::DEAD_FLYING) return;

	if (attacker) m_hitSourcePos = attacker->GetSRT().pos;

	m_state = EnemyState::KNOCKBACK;
	int oldX = m_gridX;
	int oldZ = m_gridZ;

	Unit::OnPushed(pushDir);

	// 実際に位置が変化した（壁に衝突しなかった）場合のみ、ロックオン座標をスライドさせる
	if (m_gridX != oldX || m_gridZ != oldZ) {
		if (m_isCharging) {
			// 移動量（新位置 - 旧位置）を現在のロックオン座標に加算
			m_lockedGridX += (m_gridX - oldX);
			m_lockedGridZ += (m_gridZ - oldZ);
		}
	}
}

void Enemy::TakeDamage(int damage, Unit* attacker) {
	Unit::TakeDamage(damage, attacker);
	if (attacker) m_hitSourcePos = attacker->GetSRT().pos;

	if (m_currentHP <= 0 && m_state != EnemyState::DEAD_FLYING)
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
	Unit* target = nullptr;
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

	if (distToAlly < distToPlayer) target = ally;
	else target = player;

	if (!target) {
		EndTurn();//ターゲットがいない場合、ターン終了
		return;
	}

	int dist = GetMap()->CalculateDistance(this->m_gridX, this->m_gridZ, target->GetUnitGridX(), target->GetUnitGridZ());
	//攻撃範囲の設定


	if (dist <= ATTACK_RANGE) {
		StartCharge(target);
	}
	else {
		MapManager* map = GetMap();
		int tx = target->GetUnitGridX(), tz = target->GetUnitGridZ();

		// targetをgoalとして扱う：FindPathsはtarget隣接マスで停止する（意図した仕様）
		auto path = map->FindPaths(m_gridX, m_gridZ, tx, tz, false);
		if (path.empty()) { EnemyEndAction(); return; }   // 到達不可：行動終了、残留pathを参照してonMoveFinishedを呼ばない

		// 「行動力の範囲内」で、目標に最も近づける移動先を選択
		int limit = std::min((int)path.size(), m_currentMovePoints);
		int bestIdx = -1;
		int bestD = map->CalculateDistance(m_gridX, m_gridZ, tx, tz); // 初期位置の距離を基準にする

		for (int i = 0; i < limit; ++i) {
			int d = map->CalculateDistance(path[i]->gridX, path[i]->gridZ, tx, tz);
			if (d < bestD) { bestD = d; bestIdx = i; }    // より近づく地点のみ採用
		}

		if (bestIdx < 0) { EnemyEndAction(); return; }    // 範囲内に近づける地点なし：移動しない（残留pathを参照しない）

		path.resize(bestIdx + 1);                         // 「最も近い移動先」まで経路を切り詰める
		m_currentMovePoints -= (int)path.size();

		EnemyStartMoveTo(std::move(path));
	}
}

void Enemy::EnemyEndAction() {
	m_state = EnemyState::IDLE;
}

void Enemy::EnemyStartMoveTo(std::vector<Tile*> path) {
	if (path.empty()) {
		OnMoveFinished();
		return;
	}
	Tile* oldTile = GetMap()->GetTile(m_gridX, m_gridZ);
	if (oldTile && oldTile->occupant == this) oldTile->occupant = nullptr;

	m_currentPath = std::move(path);
	m_pathIndex = 0;
	m_state = EnemyState::MOVING;
	m_targetWorldPos = GetMap()->GetWorldPosition(*m_currentPath[m_pathIndex]);
}

void Enemy::UpdateMove(float deltaSeconds) {
	Vector3 currentPos = this->GetSRT().pos;
	Vector3 direction = m_targetWorldPos - currentPos;
	SetFacingFromVector(direction);

	float distanceSq = direction.LengthSquared();
	if (distanceSq < ARRIVE_EPSILON_SQ) {
		this->SetPosition(m_targetWorldPos);
		++m_pathIndex;
		if (m_pathIndex >= m_currentPath.size()) OnMoveFinished();
		else m_targetWorldPos = GetMap()->GetWorldPosition(*m_currentPath[m_pathIndex]);
	}
	else {
		float moveStep = m_moveSpeed * deltaSeconds;
		float dist = std::sqrt(distanceSq);
		if (dist > 0.0001f) {
			direction = direction * (1.0f / dist);
			if (moveStep >= dist) this->SetPosition(m_targetWorldPos);
			else this->SetPosition(currentPos + direction * moveStep);
		}
		UpdateWorldMatrix();
	}
}

void Enemy::OnMoveFinished() {
	m_state = EnemyState::IDLE;
	m_moveRangeTiles.clear();

	if (!m_currentPath.empty()) {
		Tile* endTile = m_currentPath.back();
		this->m_gridX = endTile->gridX;
		this->m_gridZ = endTile->gridZ;
		if (endTile) {
			endTile->occupant = this;
			//罠のチェック
			if (endTile->structure) endTile->structure->OnEnter(this);
		}
	}
	//プレーヤーと味方の距離を再計算、攻撃範囲内かどうか
	Player* player = m_context->GetPlayer();
	Ally* ally = m_context->GetAlly();
	Unit* targetInRange = nullptr;

	// チェック Playerは範囲内か
	if (this->CanTarget(player) > 0 && !player->IsInvincible()) {
		int d = GetMap()->CalculateDistance(m_gridX, m_gridZ, player->GetUnitGridX(), player->GetUnitGridZ());
		if (d <= ATTACK_RANGE) targetInRange = player;
	}
	// チェック Ally は範囲内か(優先でAllyロック)
	if (this->CanTarget(ally) && !ally->IsInvincible()) {
		int d = GetMap()->CalculateDistance(m_gridX, m_gridZ, ally->GetUnitGridX(), ally->GetUnitGridZ());
		if (d <= ATTACK_RANGE) targetInRange = ally;
	}

	if (targetInRange) {
		StartCharge(targetInRange);
		return;
	}
	EnemyEndAction();
}

void Enemy::StartCharge(Unit* target) {
	if (target) {
		m_lockedGridX = target->GetUnitGridX();
		m_lockedGridZ = target->GetUnitGridZ();
		// 前の段階で残っていた反転（フリップ）状態を強制的に中断し、SetFacing が確実に適用されるようにする
		m_isTurning = false;
		//向きの再設定
		Vector3 myPos = GetMap()->GetWorldPosition(m_gridX, m_gridZ);
		Vector3 targetPos = GetMap()->GetWorldPosition(m_lockedGridX, m_lockedGridZ);
		SetFacingFromVector(targetPos - myPos);
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
	m_state = EnemyState::DEAD_FLYING;
	// 死亡時に蓄力（チャージ）状態を強制クリアし、警告UIの残存を防止する
	m_isCharging = false;
	m_pendingCharge = false;

	if (m_context && GetMap()) {
		Tile* myTile = GetMap()->GetTile(m_gridX, m_gridZ);
		if (myTile && myTile->occupant == this) myTile->occupant = nullptr;
	}

	StartDeathFly();

	if (m_context && m_context->GetCamera())
		m_context->GetCamera()->PlayKillCam(m_hitSourcePos, m_srt.pos,true);
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

	// --- 移動中かつ移動範囲データが存在する場合、薄い緑色で描画 ---
	if (m_state == EnemyState::MOVING && m_context && GetMap() && !m_moveRangeTiles.empty()) {
		GetMap()->DrawColoredTiles(m_moveRangeTiles, MOVE_RANGE_COLOR);
	}

	// 敵がチャージ中の場合、ターゲットとなる床を赤くハイライトする
	if (m_isCharging && m_context && GetMap()) {
		Tile* targetTile = GetMap()->GetTile(m_lockedGridX, m_lockedGridZ);
		if (targetTile) {
			std::vector<Tile*> dangerTiles = { targetTile };
			GetMap()->DrawColoredTiles(dangerTiles, DANGER_TILE_COLOR);
		}
	}
}

void Enemy::OnDrawOverlay(float /*deltaSeconds*/) {
	if (m_currentHP <= 0) return;

	// 敵の攻撃意図（赤い矢印）を最前面に描画
	if (m_isCharging && m_attackArrowRenderer && m_context && GetMap()) {
		if (m_enemyShader) m_enemyShader->SetGPU();

		Vector3 myPos = GetMap()->GetWorldPosition(m_gridX, m_gridZ);
		Vector3 targetPos = GetMap()->GetWorldPosition(m_lockedGridX, m_lockedGridZ);
		Vector3 arrowPos = myPos + (targetPos - myPos) * ATTACK_ARROW_POS_RATIO;
		arrowPos.y += ZFight::EnemyArrow; // Overlayレイヤー内での浮かせ具合を調整

		Vector3 diff = targetPos - myPos;
		float rotY = 0.0f;
		if (diff.x > AXIS_BIAS_THRESHOLD)       rotY = 0.0f;
		else if (diff.x < -AXIS_BIAS_THRESHOLD) rotY = PI;
		else if (diff.z > AXIS_BIAS_THRESHOLD)  rotY = -PI / 2.0f;
		else if (diff.z < -AXIS_BIAS_THRESHOLD) rotY = PI / 2.0f;

		Matrix4x4 world = Matrix4x4::CreateScale(Vector3(ATTACK_ARROW_SCALE, ATTACK_ARROW_SCALE, ATTACK_ARROW_SCALE))
			* Matrix4x4::CreateRotationY(rotY)
			* Matrix4x4::CreateTranslation(arrowPos);

		Renderer::SetWorldMatrix(&world);
		if (auto* mat = m_attackArrowRenderer->GetMaterial(0)) {
			MATERIAL old = mat->GetData();
			MATERIAL temp = old;
			temp.Diffuse = ATTACK_ARROW_COLOR;
			mat->SetMaterial(temp);
			m_attackArrowRenderer->Draw();
			mat->SetMaterial(old);
		}

		// ターゲットのタイルに誰かが存在し、かつそれが自分自身でない場合、
		
		// そのユニットがどこへ押し出されるかのプレビューを継続的に表示する
		Tile* lockedTile = GetMap()->GetTile(m_lockedGridX, m_lockedGridZ);
		if (lockedTile && this->CanTarget(lockedTile->occupant)) {
			lockedTile->occupant->DrawPushPreview(m_facing);
		}
	}
}

void Enemy::ReleaseChargeAttack() {
	m_state = EnemyState::ATTACKING;
	Vector3 targetGridPos = GetMap()->GetWorldPosition(m_lockedGridX, m_lockedGridZ);
	StartAttackAnimation(targetGridPos);
	m_attackTimer = 0.0f;
	m_isCharging = false;

	if (m_context && m_context->GetCamera())
		m_context->GetCamera()->PlayAttackZoom((m_srt.pos + targetGridPos) * 0.5f);
}