#include	"enemy.h"	
#include    "../system/CDirectInput.h"
#include	"../system/meshmanager.h"
#include	"../system/motiondatamanager.h"
#include	"../manager/GameContext.h"
#include	"../scene/GameScene.h"
#include	"../manager/MapManager.h"
#include "../ui/DamageNumberManager.h"
#include    <iostream>
#include	<random>
#include "../manager/EffectManager.h"

std::random_device rd;
std::mt19937 gen(rd());
std::uniform_real_distribution<float> shake_dist(-0.01f, 0.01f);
std::uniform_real_distribution<float> death_spin_dist(0, 10.0f);//均等の分布（０～１０）


void Enemy::init(){}

void Enemy::Init(int sequenceNumber)
{
	std::string numStr = std::to_string(sequenceNumber);
	std::string frontName = "enemy_front_" + numStr;
	std::string backName = "enemy_back_" + numStr;

	// ディレクトリパス
	std::string dirPath = "assets/model/character/";

	// 正面（Front）モデルのロード ---
	{
		// 完全なファイルパスを構築
		std::string fullPath = dirPath + frontName + ".obj";

		// ファイルが本当に存在するかチェックする
		if (!std::filesystem::exists(fullPath)) {
			// コンソールが無い場合でもVSの出力ウィンドウに出るようにする
			std::string err = "[Error] File NOT Found: " + fullPath + "\n";
			OutputDebugStringA(err.c_str());
			MessageBoxA(NULL, err.c_str(), "File Load Error", MB_OK | MB_ICONERROR);
			// ここで return しないとクラッシュします
			return;
		}

		// デバッグ用：ロードしようとしているパスを出力
		std::string msg = "[Log] Loading: " + fullPath + "\n";
		OutputDebugStringA(msg.c_str());

		std::unique_ptr<CStaticMesh> mesh = std::make_unique<CStaticMesh>();

		// 構築済みの fullPath を渡す
		mesh->Load(fullPath.c_str(), dirPath.c_str());

		// --- ここから下は変更なし ---
		std::unique_ptr<CStaticMeshRenderer> renderer = std::make_unique<CStaticMeshRenderer>();
		renderer->Init(*mesh);
		MeshManager::RegisterMesh<CStaticMesh>(frontName, std::move(mesh));
		MeshManager::RegisterMeshRenderer<CStaticMeshRenderer>(frontName, std::move(renderer));
	}

	// 背面（Back）モデルのロード
	{
		// 完全なファイルパスを構築
		std::string fullPath = dirPath + backName + ".obj";

		// ファイルが本当に存在するかチェックする
		if (!std::filesystem::exists(fullPath)) {
			// コンソールが無い場合でもVSの出力ウィンドウに出るようにする
			std::string err = "[Error] File NOT Found: " + fullPath + "\n";
			OutputDebugStringA(err.c_str());
			MessageBoxA(NULL, err.c_str(), "File Load Error", MB_OK | MB_ICONERROR);
			// ここで return しないとクラッシュします
			return;
		}

		// デバッグ用：ロードしようとしているパスを出力
		std::string msg = "[Log] Loading: " + fullPath + "\n";
		OutputDebugStringA(msg.c_str());

		std::unique_ptr<CStaticMesh> mesh = std::make_unique<CStaticMesh>();

		// 構築済みの fullPath を渡す
		mesh->Load(fullPath.c_str(), dirPath.c_str());

		// --- ここから下は変更なし ---
		std::unique_ptr<CStaticMeshRenderer> renderer = std::make_unique<CStaticMeshRenderer>();
		renderer->Init(*mesh);
		MeshManager::RegisterMesh<CStaticMesh>(backName, std::move(mesh));
		MeshManager::RegisterMeshRenderer<CStaticMeshRenderer>(backName, std::move(renderer));
	}

	// シェーダーの取得（ライティング無効の Unlit シェーダーを使用）
	m_EnemyShader = MeshManager::getShader<CShader>("unlightshader");

	//  Unit クラス（基底クラス）への登録
	auto* frontR = MeshManager::getRenderer<CStaticMeshRenderer>(frontName);
	auto* backR = MeshManager::getRenderer<CStaticMeshRenderer>(backName);
	SetModelRenderers(frontR, backR);

	//行動順番UIの初期化
	m_actionUI = std::make_unique<EnemyActionUI>();
	m_actionUI->Init(m_context);
	
	m_srt.scale = Vector3(1.0f, 1.0f, 1.0f);
	m_srt.rot = Vector3(0, 0, 0);

	m_targetWorldPos = m_srt.pos;
	m_moveSpeed = 5.0f;

	m_maxMovePoints = 4;
	m_currentMovePoints = m_maxMovePoints;

	m_maxHP = 6;
	m_currentHP = m_maxHP;

	m_team = Team::Enemy;



	m_state = EnemyState::IDLE;
	m_isDead = false;
	//初期向き：プレイヤーの方を向く
	// デフォルトの初期状態として【南】を向かせる
	// これにより、目標が「西」になった場合に「南 -> 西」の回転アニメーションが再生される

	

	//  目標方向の計算（水平方向優先の重み付けロジック）
	Player* target = m_context->GetPlayer();
	if (target)
	{
		Vector3 myPos = m_context->GetMapManager()->GetWorldPosition(m_gridX, m_gridZ);
		Vector3 targetPos = m_context->GetMapManager()->GetWorldPosition(target->GetUnitGridX(), target->GetUnitGridZ());

		// ベクトル：自分 -> ターゲット (targetPos - myPos = 前進方向)
		Vector3 diff = targetPos - myPos;

		Direction finalDir = Direction::South;

		// --- 【重要】水平方向優先 (Horizontal Bias) の判定 ---
		// 通常、斜め（例：-2, -3）の場合は数値の大きい South が判定されるが、
		// 視覚的にはプレイヤーを側面的に捉えた方が自然なため、X軸に重みを持たせる。
		if (std::abs(diff.x) > 0.1f)
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
	}

	// プッシュ方向の矢印メッシュのレンダラーを取得
	m_pushArrowRenderer = MeshManager::getRenderer<CStaticMeshRenderer>("arrow_push_mesh");

	// 攻撃方向の矢印メッシュのレンダラーを取得
	m_attackArrowRenderer = MeshManager::getRenderer<CStaticMeshRenderer>("arrow_attack_mesh");
	if (!m_attackArrowRenderer) {
		m_attackArrowRenderer = m_pushArrowRenderer;
	}

	UpdateWorldMatrix(); // 初期行列を確定させる
}

void Enemy::Update(uint64_t dt) {

	float deltaSeconds = static_cast<float>(dt) / 1000.0f;

	// 基底クラスの反転（フリップ）アニメーション処理を実行
	UpdateFlipAnimation(deltaSeconds);

	if (m_pendingCharge) {
		// 蓄力（突進準備）が保留されており、かつ反転アニメーションが完了（false）した場合
		if (!m_isFlipping) {
			m_pendingCharge = false; // 待機フラグを解除
			m_isCharging = true;     // 蓄力（震え演出）を正式に開始
			std::cout << "[Enemy] Turn complete. Now Charging!" << std::endl;
		}
	}

	//死亡飛翔中の更新処理
	if (m_state == EnemyState::DEAD_FLYING)
	{
		DeathFlyingUpdate(deltaSeconds);
		return; // これ以降のAIや移動処理はさせない
	}

	if (m_isMyTurn && m_state == EnemyState::IDLE)
	{
		ExecuteAI();
		m_isMyTurn = false;
	}
	//行動順番UIの更新
	if (m_actionUI) {
		m_actionUI->Update(deltaSeconds);
	}

	switch (m_state)
	{
	case EnemyState::MOVING:
		updateMove(dt);
		break;
	case EnemyState::ATTACKING:

		m_attackTimer += deltaSeconds;

		if (m_attackTimer > 0.1f)
		{
			auto impactCallback = [this]() {//this->ここのクラス
				//ロックしたタイルを取得
				Tile* targetTile = m_context->GetMapManager()->GetTile(m_lockedGridX, m_lockedGridZ);

				if (targetTile != nullptr && targetTile->occupant != nullptr && targetTile->occupant != this) 
				{
					std::cout << "[Enemy] SMASH!" << std::endl;
					targetTile->occupant->TakeDamage(2, this);
				}
				else {//miss

					std::cout << "[Enemy] MISSED! No one at " << m_lockedGridX << "," << m_lockedGridZ << std::endl;

					if (m_context->GetDamageManager()) 
					{
						Vector3 missPos = m_context->GetMapManager()->GetWorldPosition(m_lockedGridX, m_lockedGridZ);
						missPos.y += 1.0f;
						m_context->GetDamageManager()->SpawnDamage(missPos, 0);
					}
					}
				};
			if(UpdateAttackAnimation(dt,impactCallback)){
				//アタックアニメ実行、完了の検査
			m_state = EnemyState::IDLE;
			m_isCharging = false;
			EnemyEndAction();
			}
		}
			break;
	case EnemyState::KNOCKBACK:
		if (m_slideEndPos.LengthSquared() > 0.001f) 
		{
			if(UpdateSlideAnimation(dt))
			{
				m_state = EnemyState::IDLE;
				m_slideEndPos = Vector3(0, 0, 0);
				//ノックバック終了後、タイルイベントの検査
				Tile* currentTile = m_context->GetMapManager()->GetTile(m_gridX, m_gridZ);
				if (currentTile && currentTile->structure) {
					std::cout << "[Enemy] Knockback finished. Checking tile event..." << std::endl;
					currentTile->structure->OnEnter(this);
				}
			}
		}
		else 
		{
			if (UpdateAttackAnimation(dt, nullptr)) {
				m_state = EnemyState::IDLE;
			}
		}
		break;
	default:
		break;
	}



	UpdateWorldMatrix();
}

// 1. エンティティレイヤー (5.1)
void Enemy::OnDraw(uint64_t dt) {
	if (m_isCharging) {
		// チャージ中のアニメーション描画
		ChargeAnimation();
	}
	else {
		Renderer::SetPixelArtMode(true);
		if (m_EnemyShader != nullptr) m_EnemyShader->SetGPU();
		DrawModel();
		Renderer::SetPixelArtMode(false);
	}
}

// 2. 床面 UI レイヤー (3)
void Enemy::OnDrawFloorUI(uint64_t dt) {
	// 既に死亡している場合、UIを一切描画しない
	if (m_currentHP <= 0) return;
	// 敵がチャージ中の場合、ターゲットとなる床を赤くハイライトする
	if (m_isCharging && m_context && m_context->GetMapManager()) {
		Tile* targetTile = m_context->GetMapManager()->GetTile(m_lockedGridX, m_lockedGridZ);
		if (targetTile) {
			std::vector<Tile*> dangerTiles = { targetTile };
			m_context->GetMapManager()->DrawColoredTiles(dangerTiles, Color(0.9f, 0.0f, 0.0f, 0.5f));
		}
	}
}

// 3. 浮遊 Overlay レイヤー (6)
void Enemy::OnDrawOverlay(uint64_t dt) {
	// 既に死亡している場合、UIを一切描画しない
	if (m_currentHP <= 0) return;
	// 敵の攻撃意図（赤い矢印）を最前面に描画
	if (m_isCharging && m_attackArrowRenderer && m_context && m_context->GetMapManager()) {
		if (m_EnemyShader != nullptr) m_EnemyShader->SetGPU(); // シェーダーの有効化を確認

		Vector3 myPos = m_context->GetMapManager()->GetWorldPosition(m_gridX, m_gridZ);
		Vector3 targetPos = m_context->GetMapManager()->GetWorldPosition(m_lockedGridX, m_lockedGridZ);
		Vector3 arrowPos = myPos + (targetPos - myPos) * 0.35f;
		arrowPos.y += 0.15f; // Overlayレイヤー内での浮かせ具合を調整

		Vector3 diff = targetPos - myPos;
		float rotY = 0.0f;
		if (diff.x > 0.1f)      rotY = 0.0f;
		else if (diff.x < -0.1f) rotY = PI;
		else if (diff.z > 0.1f)  rotY = -PI / 2.0f;
		else if (diff.z < -0.1f) rotY = PI / 2.0f;

		Matrix4x4 world = Matrix4x4::CreateScale(Vector3(0.6f, 0.6f, 0.6f)) * Matrix4x4::CreateRotationY(rotY) * Matrix4x4::CreateTranslation(arrowPos);

		Renderer::SetWorldMatrix(&world);
		if (auto* mat = m_attackArrowRenderer->GetMaterial(0)) {
			MATERIAL old = mat->GetData();
			MATERIAL temp = old;
			temp.Diffuse = Color(1.0f, 0.08f, 0.57f, 0.7f); // 赤色半透明で危険を通知
			mat->SetMaterial(temp);
			m_attackArrowRenderer->Draw();
			mat->SetMaterial(old);
		}
	}
}

void Enemy::dispose() {

}



void Enemy::EnemyStartAction()
{
	if (m_currentHP <= 0 || m_state == EnemyState::DEAD_FLYING) {
		return;
	}
	std::cerr << "[Enemy] ID: " << m_displayOrder << " Start Action!" << std::endl;

	if (m_isCharging)
	{
		ReleaseChargeAttack();
	}
	else
	{
		m_pendingCharge = false;
		ResetMovePoints();
		ExecuteAI();
	}

}

void Enemy::EnemyEndAction()
{
	std::cerr << "enemy turn end " << std::endl;
	m_state = EnemyState::IDLE;

}
void Enemy::ExecuteAI()
{
	std::cerr << "enemy ExecuteAI" << std::endl;
	//ターゲットのプレイヤーを取得
	Player* player = m_context->GetPlayer();
	//ターゲットの味方を取得
	Ally* ally = m_context->GetAlly();

	//ターゲット決定(一番近いユニット)
	Unit* target = nullptr;
	int distToPlayer = 999;
	int distToAlly = 999;
	//プレイヤーの距離計算
	if (player && player->GetHP() > 0) {
		distToPlayer = m_context->GetMapManager()->CalculateDistance(
			this->m_gridX, this->m_gridZ, player->GetUnitGridX(), player->GetUnitGridZ());
	}
	//味方の距離計算
	if (ally && ally->GetHP() > 0) {
		distToAlly = m_context->GetMapManager()->CalculateDistance(
			this->m_gridX, this->m_gridZ, ally->GetUnitGridX(), ally->GetUnitGridZ());
	}

	// 一番近いユニットをターゲットに設定
	if (distToAlly < distToPlayer) {
		target = ally;
		std::cout << "[AI] Target locked: ALLY (Dist: " << distToAlly << ")" << std::endl;
	}
	else {
		target = player;
		std::cout << "[AI] Target locked: PLAYER (Dist: " << distToPlayer << ")" << std::endl;
	}

	if(!target)
	{
		EndTurn();//ターゲットがいない場合、ターン終了
		return;
	}

	int dist = m_context->GetMapManager()->CalculateDistance
	(this->m_gridX, this->m_gridZ, target->GetUnitGridX(), target->GetUnitGridZ());
	//攻撃範囲の設定
	int attackRange = 1;

	if (dist <= attackRange) 
	{
		std::cout << "[AI] Attacking!" << std::endl;
		StartCharge(target);
	}
	else
	{
		std::cout << "[AI] Pathfinding..." << std::endl;
		MapManager* map = m_context->GetMapManager();
		auto path = map->FindPaths(this->m_gridX, this->m_gridZ, target->GetUnitGridX(), target->GetUnitGridZ(),false);
		//見つかった道の長さと移動力を比較する、そして移動力と同じ長さになる
		if (path.size() > m_currentMovePoints) 
		{
			path.resize(m_currentMovePoints);
		}
		m_currentMovePoints -= (int)path.size();
		EnemyStartMoveTo(path);
	}
}


void Enemy::OnTurnChanged(TurnState state)
{
	std::cerr << "enemy turn OnTurnChanged " << std::endl;
	if (state == TurnState::EnemyPhase)
	{
		//プレイヤーのターン開始
		StartTurn();
	}

}

void Enemy::TakeDamage(int damage, Unit* attacker) 
{
	std::cout << "[Debug] Enemy::TakeDamage Called! Damage: " << damage
		<< " CurrentHP: " << m_currentHP << std::endl;
	Unit::TakeDamage(damage, attacker);
	std::cout << "[Debug] HP After: " << m_currentHP << std::endl;
	if(m_currentHP<=0&&m_state != EnemyState::DEAD_FLYING)
	{
		std::cout << "[Debug] Enemy died! Switching to DEAD_FLYING." << std::endl;
		m_state = EnemyState::DEAD_FLYING;

		// 【新規追加】死亡時に蓄力（チャージ）状態を強制クリアし、警告UIの残存を防止する
		m_isCharging = false;
		m_pendingCharge = false;

		//元にいる、すでに死んだ敵のタイルの占有を解除
		if (m_context && m_context->GetMapManager()) {
			Tile* myTile = m_context->GetMapManager()->GetTile(m_gridX, m_gridZ);
			if (myTile && myTile->occupant == this) {
				myTile->occupant = nullptr;
			}
		}

		Vector3 flyDir(0, 1, 0);
		//死亡時の飛翔方向を計算(攻撃者の位置に基づく、反方向へ)
		if (attacker) 
		{	//位置を取得
			Vector3 attackerPos = attacker->getSRT().pos;
			Vector3 myPos = m_srt.pos;
			//水平方向の差分ベクトル
			Vector3 diff = myPos - attackerPos;
			diff.y = 0.0f;

			if (diff.LengthSquared() > 0.001f) {
				diff.Normalize();
			}
			else {
				diff = Vector3(0, 0, 1);
			}

			//x,zは水平方向の成分、yは上方向の成分
			flyDir = Vector3(diff.x * 1.8f, 0.6f, diff.z * 1.8f);
		}
		//初速度設定
		float force = 20.0f;
		m_deathVelocity = flyDir * force;

		//回転軸設定(ランダム)
		m_deathSpin = Vector3(death_spin_dist(gen), death_spin_dist(gen), death_spin_dist(gen));
	}

}



void Enemy::EnemyStartMoveTo(std::vector<Tile*> path)
{
	std::cerr << "enemy start to move " << std::endl;
	if (path.empty()) 
	{
		onMoveFinished();
		return;
	}
	//start点のタイル占有取り消せ
	Tile* oldTile = m_context->GetMapManager()->GetTile(m_gridX, m_gridZ);
	if (oldTile && oldTile->occupant == this) {
		oldTile->occupant = nullptr;
	}

	m_currentPath = path;
	m_pathIndex = 0;

	m_state = EnemyState::MOVING;

	m_targetWorldPos = m_context->GetMapManager()->GetWorldPosition(*m_currentPath[m_pathIndex]);

}

void Enemy::updateMove(uint64_t delta)
{
	Vector3 currentPos = this->getSRT().pos;
	Vector3 direction = m_targetWorldPos - currentPos;

	SetFacingFromVector(direction);

	float distanceSq = direction.LengthSquared();

	if (distanceSq < 0.01f) 
	{
		this->setPosition(m_targetWorldPos);
		m_pathIndex++;

		if (m_pathIndex >= m_currentPath.size()) 
		{
			onMoveFinished();
		}
		else 
		{
			m_targetWorldPos = m_context->GetMapManager()->GetWorldPosition(*m_currentPath[m_pathIndex]);
			//LookAt向き調整待ち
		}
	}
	else 
	{
	
		float dt = static_cast<float>(delta) / 1000.0f;
		float moveStep = m_moveSpeed * dt;

		float dist = std::sqrt(distanceSq);
		if (dist > 0.0001f) 
		{
			direction = direction * (1.0f / dist);

			if (moveStep >= dist) 
			{
				this->setPosition(m_targetWorldPos);
			}
			else
			{
				Vector3 newPos = currentPos + direction * moveStep;
				this->setPosition(newPos);
			}

		}

		//向き調整待ち
		UpdateWorldMatrix();
	}
}

void Enemy::onMoveFinished()
{
	std::cerr << "enemy move finished " << std::endl;
	m_state = EnemyState::IDLE;
	//移動終了後、最後のタイルに位置を更新
	if (!m_currentPath.empty()) 
	{
		Tile* endTile = m_currentPath.back();
		this->m_gridX = endTile->gridX;
		this->m_gridZ = endTile->gridZ;
		//ゴールのタイルを占有
		if (endTile) {
			endTile->occupant = this;
			//罠のチェック
			if (endTile->structure) {
				std::cout << "[Enemy] Movement finished. Checking tile event..." << std::endl;
				endTile->structure->OnEnter(this);
			}
		}

	}
	//プレーヤーと味方の距離を再計算、攻撃範囲内かどうか
	Player* player = m_context->GetPlayer();
	Ally* ally = m_context->GetAlly();
	//ターゲット決定(一番近いユニット)
	Unit* targetInRange = nullptr;

	// チェック Playerは範囲内か
	if (player && player->GetHP() > 0) {
		int d = m_context->GetMapManager()->CalculateDistance(m_gridX, m_gridZ, player->GetUnitGridX(), player->GetUnitGridZ());
		if (d <= 1) targetInRange = player;
	}
	// チェック Ally は範囲内か(優先でAllyロック)
	if (ally && ally->GetHP() > 0) {
		int d = m_context->GetMapManager()->CalculateDistance(m_gridX, m_gridZ, ally->GetUnitGridX(), ally->GetUnitGridZ());
		if (d <= 1) targetInRange = ally;
	}
	//攻撃範囲内のターゲットがいる場合、チャージ攻撃開始
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

		std::cerr << "[Enemy] Locked Target Grid: " << m_lockedGridX << "," << m_lockedGridZ << std::endl;
		//向きの再設定
		Vector3 myPos = m_context->GetMapManager()->GetWorldPosition(m_gridX, m_gridZ);
		Vector3 targetPos = m_context->GetMapManager()->GetWorldPosition(m_lockedGridX, m_lockedGridZ);
		SetFacingFromVector(targetPos - myPos);
	}
	// 基底クラスの m_isFlipping フラグを確認（Unit.h で定義、Enemy からアクセス可能）
	if (m_isFlipping) {
		// 反転アニメーション中の場合は、突進前の「震え演出」を保留にする
		m_pendingCharge = true;
		m_isCharging = false;
		std::cout << "[Enemy] Turning... Waiting to charge." << std::endl;
	}
	else {
		// 既に対象の方向を向いている（反転不要な）場合は、即座に震え演出を開始
		m_pendingCharge = false;
		m_isCharging = true;
		std::cout << "[Enemy] Facing correct. Start charging immediately." << std::endl;
	}

	EnemyEndAction();
}

void Enemy::ReleaseChargeAttack() {
	std::cerr << "[Enemy] Releasing Charged Attack!" << std::endl;

	m_state = EnemyState::ATTACKING;

	Vector3 targetGridPos = m_context->GetMapManager()->GetWorldPosition(m_lockedGridX, m_lockedGridZ);
	StartAttackAnimation(targetGridPos);
	m_attackTimer = 0.0f;

	m_isCharging = false;

}

void Enemy::ChargeAnimation() {
	Vector3 originalPos = m_srt.pos;

	if (m_isCharging) {
		float shakeX = shake_dist(gen);
		float shakeZ = shake_dist(gen);

		m_srt.pos.x += shakeX;
		m_srt.pos.z += shakeZ;

		UpdateWorldMatrix();
		Renderer::SetWorldMatrix(&m_WorldMatrix);
	}

	if (m_EnemyShader)m_EnemyShader->SetGPU();
	DrawModel();

	if (m_isCharging) {
		m_srt.pos = originalPos;
		UpdateWorldMatrix();
		Renderer::SetWorldMatrix(&m_WorldMatrix);
	}
}

void Enemy::OnPushed(Direction pushDir)
{
	if (m_currentHP <= 0 || m_state == EnemyState::DEAD_FLYING) {
		return;
	}

	//プッシュ向き入れた後、目標タイルを計算
	DirOffset offset = DirOffset::From(pushDir);
	int targetX = m_gridX + offset.x;
	int targetZ = m_gridZ + offset.z;

	//障害物の検査
	MapManager* map = m_context->GetMapManager();
	bool isBlocked = !(map->IsWalkable(targetX, targetZ));
	Tile* targetTile = map->GetTile(targetX, targetZ);

	//Unitの検査
	Unit* obstacleUnit = nullptr;
	if (targetTile && targetTile->occupant)
	{
		isBlocked = true;
		obstacleUnit = targetTile->occupant;
	}

	m_state = EnemyState::KNOCKBACK;


	if (isBlocked)
	{	//ぶつかりの処理
		Vector3 obstaclePos = map->GetWorldPosition(targetX, targetZ);
		StartBumpAnimation(obstaclePos);

		//ダメージの処理
		std::cout << "[Enemy] Hit Wall/Unit! Ouch!" << std::endl;
		this->TakeDamage(1, nullptr);

		if (obstacleUnit) {
			std::cout << "[Enemy] Crushed someone else too!" << std::endl;
			obstacleUnit->TakeDamage(1, this); // ぶつかった向こうもダメージを受ける
		}
	}
	else 
	{
		//障害物はない時の移動処理
		Tile* currentTile = map->GetTile(m_gridX, m_gridZ);
		if (currentTile)currentTile->occupant = nullptr;

		m_gridX = targetX;
		m_gridZ = targetZ;

		if(targetTile)targetTile->occupant = this;

		Vector3 targetWorldPos = map->GetWorldPosition(targetX, targetZ);
		StartSlideAnimation(targetWorldPos);
		//ロック目標の再計算
		if (m_isCharging)
		{
			// 現在の向きからオフセットを取得
			DirOffset faceOffset = DirOffset::From(m_facing);

			// 新しい位置に基づいてロック目標を再計算
			m_lockedGridX = m_gridX + faceOffset.x;
			m_lockedGridZ = m_gridZ + faceOffset.z;

			std::cout << "[Enemy] Pushed while charging! Relock Target -> "
				<< m_lockedGridX << "," << m_lockedGridZ << std::endl;
		}
	}
}

void Enemy::DeathFlyingUpdate(float deltaSeconds)
{	//ログずっと出るの防止
	if (m_isDead) return;
	//重力の影響(v = v0 + at)
		m_deathVelocity.y -= (GRAVITY * deltaSeconds);

		//位置の更新(p = p0 + vt)
		m_srt.pos += m_deathVelocity * deltaSeconds;

		//回転の更新
		m_srt.rot += m_deathSpin * deltaSeconds;

		//LOG
		static float lastPrintY = 0.0f;
		if (std::abs(m_srt.pos.y - lastPrintY) > 5.0f) {
			std::cerr << "[Enemy Debug] Flying... Y=" << m_srt.pos.y << std::endl;
			lastPrintY = m_srt.pos.y;
		}

		//境界チェック
		if (m_srt.pos.y < -20.0f) 
		{
			std::cerr << "[Enemy Debug] Y < -30! Calling Deactivate & RemoveEnemy." << std::endl; //
			
			if (m_context && m_context->GetEnemyManager()) {
				std::cerr << "[Enemy Debug] Pointer to Manager is valid. Removing..." << std::endl; 
				m_context->GetEnemyManager()->RemoveEnemy(this);
			}
			else {
				std::cerr << "[Enemy Debug] FATAL: EnemyManager is NULL!" << std::endl;
			}

			Destroy();
		}

		UpdateWorldMatrix();
		return;
}

void Enemy::SetDisplayOrder(int order)
{
	m_displayOrder = order;
}

void Enemy::DrawUI() {
	// 1. まず基底クラスのメソッドを呼び出し、HPバーを描画する
	Unit::DrawUI();

	// 2. 既に死亡している（画面外への吹き飛び中など）場合は、
	//    基本的にUIを表示しない（仕様に応じて変更可能）
	if (m_currentHP <= 0) return;

	// 3. 行動順を示す数字を描画
	if (m_actionUI) {
		// 現在の座標と表示順（オーダー）を渡して描画
		m_actionUI->Draw(m_srt.pos, m_displayOrder);
	}
}

void Enemy::DrawPushPreview(Direction pushDir) {
	if (!m_pushArrowRenderer || !m_context || !m_context->GetMapManager()) return;

	MapManager* map = m_context->GetMapManager();
	DirOffset offset = DirOffset::From(pushDir);

	// ノックバック先の対象グリッドを計算
	int targetX = m_gridX + offset.x;
	int targetZ = m_gridZ + offset.z;

	// 衝突判定（通行不可タイル、または既に占有者がいるか）
	bool isBlocked = !map->IsWalkable(targetX, targetZ);
	Tile* targetTile = map->GetTile(targetX, targetZ);
	if (targetTile && targetTile->occupant) isBlocked = true;

	// 描画位置の計算：現在のグリッドと対象グリッドの境界付近に設定
	Vector3 myPos = map->GetWorldPosition(m_gridX, m_gridZ);
	Vector3 targetPos = map->GetWorldPosition(targetX, targetZ);
	Vector3 arrowPos = myPos + (targetPos - myPos) * 0.2f;
	arrowPos.y += 0.08f; // 地面とのめり込み（Zファイティング）防止のためのオフセット

	// 回転角の計算
	float rotY = 0.0f;
	if (offset.x == 1)      rotY = 0.0f;
	else if (offset.x == -1) rotY = PI;
	else if (offset.z == 1)  rotY = -PI / 2.0f;
	else if (offset.z == -1) rotY = PI / 2.0f;

	Matrix4x4 world = Matrix4x4::CreateScale(Vector3(1.0f, 1.0f, 1.5f))
		* Matrix4x4::CreateRotationY(rotY)
		* Matrix4x4::CreateTranslation(arrowPos);

	// 衝突時は黄色（半透明）、非衝突時は灰色（半透明）
	Color arrowColor = isBlocked ? Color(1.0f, 1.0f, 0.0f, 0.7f) : Color(0.6f, 0.6f, 0.6f, 0.9f);


	Renderer::SetWorldMatrix(&world);
	if (auto* mat = m_pushArrowRenderer->GetMaterial(0)) {
		MATERIAL old = mat->GetData();
		MATERIAL temp = old;
		temp.Diffuse = arrowColor;
		mat->SetMaterial(temp);
		m_pushArrowRenderer->Draw();
		mat->SetMaterial(old); // マテリアルを元の状態に復元
	}

	// 衝突判定がある場合、矢印の後にヒットエフェクトを重ねて描画（より高い描画優先度）
	if (isBlocked && m_context->GetEffectManager()) {
		Vector3 effectPos = targetPos;
		effectPos.y += 0.8f; // ユニットの中心高さに合わせて調整
		m_context->GetEffectManager()->DrawStaticHitPreview(effectPos);
	}
}