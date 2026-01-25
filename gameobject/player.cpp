#include	"../gameobject/player.h"	
#include    "../system/CDirectInput.h"
#include	"../system/meshmanager.h"
#include	"../scene/GameScene.h"	
#include	"../manager/GameContext.h"
#include	"../manager/MapManager.h"
#include	"../ui/GameUIManager.h"
#include	 "../system//IScene.h"
#include    "../system/CSprite.h"
#include	<cmath>
#include	<iostream>


void Player::playerResourceLoader()
{
	static std::unique_ptr<CSprite> g_spr;
	std::unique_ptr<CStaticMesh> smesh = std::make_unique<CStaticMesh>();
	smesh->Load("assets/model/character/player.obj", "assets/model/character/");

	std::unique_ptr<CStaticMeshRenderer> srenderer = std::make_unique<CStaticMeshRenderer>();
	srenderer->Init(*smesh);

	MeshManager::RegisterMesh<CStaticMesh>("player_mesh", std::move(smesh));
	MeshManager::RegisterMeshRenderer<CStaticMeshRenderer>("player_mesh", std::move(srenderer));
	//パス直線
	{
		std::unique_ptr<CStaticMesh> mesh = std::make_unique<CStaticMesh>();
		mesh->Load("assets/model/obj/arrow_straight.obj", "assets/model/obj/");
		std::unique_ptr<CStaticMeshRenderer> renderer = std::make_unique<CStaticMeshRenderer>();
		renderer->Init(*mesh);
		MeshManager::RegisterMesh<CStaticMesh>("arrow_straight_mesh", std::move(mesh));
		MeshManager::RegisterMeshRenderer<CStaticMeshRenderer>("arrow_straight_mesh", std::move(renderer));
	}

	//パス直角
	{
		std::unique_ptr<CStaticMesh> mesh = std::make_unique<CStaticMesh>();
		mesh->Load("assets/model/obj/arrow_corner.obj", "assets/model/obj/"); 
		std::unique_ptr<CStaticMeshRenderer> renderer = std::make_unique<CStaticMeshRenderer>();
		renderer->Init(*mesh);
		MeshManager::RegisterMesh<CStaticMesh>("arrow_corner_mesh", std::move(mesh));
		MeshManager::RegisterMeshRenderer<CStaticMeshRenderer>("arrow_corner_mesh", std::move(renderer));
	}
}

void Player::Init() {

	playerResourceLoader();

	m_PlayerMesh = MeshManager::getMesh<CStaticMesh>("player_mesh");
	m_PlayerMeshrenderer = MeshManager::getRenderer<CStaticMeshRenderer>("player_mesh");
	m_PlayerShader = MeshManager::getShader<CShader>("unlightshader");
	m_PathLineRenderer = MeshManager::getRenderer<CStaticMeshRenderer>("arrow_straight_mesh");
	m_PathCornerRenderer = MeshManager::getRenderer<CStaticMeshRenderer>("arrow_corner_mesh");

	if(m_PlayerMesh){
		std::cerr << "Player Mesh Loaded Successfully." << std::endl;
	}
	else{
		std::cerr << "CRITICAL ERROR: 'player_mesh' not found! Check model file paths." << std::endl;
	}
	if(m_PlayerMeshrenderer){
		std::cerr << "Player MeshRenderer Loaded Successfully." << std::endl;
	}
	else{
		std::cerr << "CRITICAL ERROR: 'player_mesh' Renderer not found! Check model file paths." << std::endl;
	}
	if(m_PlayerShader){
		std::cerr << "Player Shader Loaded Successfully." << std::endl;
	}
	else{
		std::cerr << "CRITICAL ERROR: 'lightshaderSpecular' not found! Check shader file paths." << std::endl;
	}

	const int BORN_GRID_X = 1;
	const int BORN_GRID_Z = 1;
	Tile* bornTile = m_context->GetMapManager()->GetTile(BORN_GRID_X, BORN_GRID_Z);

	if(bornTile==nullptr)
	{
		bornTile = m_context->GetMapManager()->GetTile(0, 0);
		if (bornTile == nullptr) { return; }
	}

	if(bornTile)
	{
		bornTile->occupant = this;
	}

	Vector3 bornWorldPos = m_context->GetMapManager()->GetWorldPosition(*bornTile);
	m_srt.pos = bornWorldPos;

	m_gridX = bornTile->gridX;
	m_gridZ = bornTile->gridZ;


	m_srt.scale = Vector3(1.0f, 1.0f, 1.0f);
	m_srt.rot = Vector3(0, 0, 0);

	m_state = PlayerState::WAITING;
	m_targetWorldPos = m_srt.pos;
	m_moveSpeed = 5.0f;

	SetFacing(Direction::North);
	m_srt.rot.y = m_targetRot.y;
	 
	m_currentMovePoints = 4;
	m_maxMovePoints = 4;
	m_maxHP = 5;
	m_currentHP = 5;

	m_hasActioned = false;
	m_isRotating = false;

	m_team = Team::Player;

	UpdateWorldMatrix();
}

void Player::Update(uint64_t dt) {
	//ミリ秒を秒に
	float deltaSeconds = static_cast<float>(dt) / 1000.0f;
	if (m_context->GetUIManager()->IsAnimating()) {return;}

	//アニメション終わった後、またメニュー遷移実際に実行する
	//メインメニュー状態の場合
	if (m_state == PlayerState::MENU_MAIN && m_nextState != PlayerState::MENU_MAIN) {
		if (m_nextState == PlayerState::MOVE_SELECT) {
			SwitchToMoveSelect();
		}
		else if (m_nextState == PlayerState::MENU_ATTACK) {
			SwitchToAttackMenu();
		}
		else if (m_nextState == PlayerState::WAITING) {
			EndTurn();
		}
		//安全おため、もう一回リセット
		m_nextState = m_state;
	}
	// 攻撃メニュー状態の場合
	else if (m_state == PlayerState::MENU_ATTACK && m_nextState != PlayerState::MENU_ATTACK) {
		if (m_nextState == PlayerState::ATTACK_DIR_SELECT) {
			SwitchToAttackDirSelect(m_selectedAttackType);
		}
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
		// プレビュー位置の更新
		m_srt.pos = m_context->GetMapManager()->GetWorldPosition(m_previewGridX, m_previewGridZ);
		UpdateWorldMatrix();
		break;
	case PlayerState::ANIM_MOVE:

		if (UpdatePathMovement(deltaSeconds)) {
			
			m_hasMoved = true; //移動完了のフラグを立てる

			//メインメニューに切り替え
			SwitchToMenuMain();
		}
		break;
	case PlayerState::MENU_ATTACK:
		HandleAttackMenuInput();
		break;
	case PlayerState::ATTACK_DIR_SELECT:
		HandleAttackDirInput(deltaSeconds);
		m_srt.pos = m_context->GetMapManager()->GetWorldPosition(m_gridX, m_gridZ);
		break;
	case PlayerState::ANIM_ATTACK:
		if (UpdateAttackAnimation(dt, nullptr)) {
			//攻撃アニメーション終了後、ターン終了
			EndTurn(); 
		}
		break;
	case PlayerState::WAITING:
		// プレーヤーターン以外の待機状態
		break;
	
	}
	//向きの更新
	UpdatePaperOrientation();

	UpdateWorldMatrix();
}

void Player::OnDraw(uint64_t dt) {
	if (m_PlayerShader != nullptr) {
	m_PlayerShader->SetGPU();
}	
	//移動操作中のDraw
	if (m_state == PlayerState::MOVE_SELECT) {
		

		//移動範囲の緑タイル
		Renderer::SetBlendState(BS_ALPHABLEND);
		Renderer::SetDepthEnable(false);
		m_context->GetMapManager()->DrawColoredTiles(m_moveRangeTiles, Color(0, 1, 0, 0.4f));

		//移動ルート
		DrawPathLine();

		//スタート点の灰色残影
		DrawGhost();

		Renderer::SetDepthEnable(true);
		Renderer::SetBlendState(BS_NONE);
	}
	else if (m_state == PlayerState::ATTACK_DIR_SELECT) {
		DrawAttackWarning();
	}
	if (m_PlayerMeshrenderer != nullptr) {
		//プレーヤー本体は透明の部分あるため
		Renderer::SetBlendState(BS_ALPHABLEND);

		Renderer::SetDepthEnable(true);

		Renderer::SetWorldMatrix(&m_WorldMatrix);

		m_PlayerMeshrenderer->Draw();
		
		Renderer::SetBlendState(BS_NONE);
	}
}
//スタート点の半透明残影
void Player::DrawGhost() {
	if (!m_PlayerMeshrenderer) return;

	Vector3 ghostPos = m_context->GetMapManager()->GetWorldPosition(m_startGridX, m_startGridZ);
	ghostPos.y += 0.015f;

	Matrix4x4 ghostWorld = Matrix4x4::CreateScale(m_srt.scale) * Matrix4x4::CreateRotationY(m_srt.rot.y) * Matrix4x4::CreateTranslation(ghostPos);

	Renderer::SetWorldMatrix(&ghostWorld);

	// マテリアルを灰色半透明に変更して描画
	CMaterial* mtrl = m_PlayerMeshrenderer->GetMaterial(0);
	MATERIAL old = mtrl->GetData();
	MATERIAL gray = old;
	gray.Diffuse = Color(1.0f, 1.0f, 1.0f, 0.6f); //灰色半透明
	mtrl->SetMaterial(gray);

	Renderer::SetBlendState(BS_ALPHABLEND);
	Renderer::SetDepthEnable(false);
	m_PlayerMeshrenderer->Draw();
	Renderer::SetDepthEnable(true);
	Renderer::SetBlendState(BS_NONE);

	mtrl->SetMaterial(old); //マテリアルをリセット

	//WorldMatrixを元に戻す
	UpdateWorldMatrix();
	Renderer::SetWorldMatrix(&m_WorldMatrix);
}

float Player::CalculateLineRotation(int dx, int dz) {
	// dx, dzはパスの方向
	if (dx == 1) return 0.0f;           // 右（デフォルト）
	if (dx == -1) return PI;            // 左 (180度)
	if (dz == 1) return -PI / 2.0f;     // 上 (-90度、反時計回り90度にZ+を指す)
	if (dz == -1) return PI / 2.0f;     // 下 (+90度)
	return 0.0f;
}
float Player::CalculateCornerRotation(int dx1, int dz1, int dx2, int dz2) {
	// dx1, dz1: 隣タイル１と現在のタイルの相対中心偏移
	// dx2, dz2: 隣タイル２と現在のタイルの相対中心偏移

	//繋がっているの二つの方向を判断
	bool left = (dx1 == -1 || dx2 == -1);
	bool right = (dx1 == 1 || dx2 == 1);
	bool up = (dz1 == 1 || dz2 == 1);
	bool down = (dz1 == -1 || dz2 == -1);

	// デフォルト：Left + Down -> Rot 0
	if (left && down) return 0.0f;

	// Down + Right -> Rot -90 (270)
	if (down && right) return -PI / 2.0f;

	// Right + Up -> Rot 180
	if (right && up) return PI;

	// Up + Left -> Rot 90
	if (up && left) return PI / 2.0f;

	return 0.0f;
}
//移動パスの描画
void Player::DrawPathLine() {
	//スタートポイントは描画しない
	if (m_currentPath.empty()) return;
	//startTileを取得
	Tile* startTile = m_context->GetMapManager()->GetTile(m_startGridX, m_startGridZ);
	//スタートポイントので、i=1から
	for (size_t i = 0; i < m_currentPath.size(); ++i) {
		Tile* currTile = m_currentPath[i];
		//スタートポイントは描画しない
		if (currTile->gridX == m_startGridX && currTile->gridZ == m_startGridZ) continue;

		Tile* prevTile = nullptr;
		if (i == 0) {
			// もし i=0 なら、前の点はスタートポイント
			prevTile = startTile;
		}
		else {
			// 通常の前の点
			prevTile = m_currentPath[i - 1];
		}

		Tile* nextTile = (i + 1 < m_currentPath.size()) ? m_currentPath[i + 1] : nullptr;

		//位置取得
		Vector3 pos = m_context->GetMapManager()->GetWorldPosition(currTile->gridX, currTile->gridZ);
		pos.y += 0.05f; //地面よりちょっと高い

		float rotY = 0.0f;
		CStaticMeshRenderer* rendererToUse = nullptr;

		//モデルの種類と回転を確認
		if (nextTile) {
			//中間タイル
			//入るの方向(Prev -> Curr)
			int dxIn = currTile->gridX - prevTile->gridX;
			int dzIn = currTile->gridZ - prevTile->gridZ;

			// 出るの方向(Curr -> Next)
			int dxOut = nextTile->gridX - currTile->gridX;
			int dzOut = nextTile->gridZ - currTile->gridZ;

			if (dxIn == dxOut && dzIn == dzOut) {
				// 直線：もし入ると出るの方向が一致なら
				rendererToUse = m_PathLineRenderer;
				rotY = CalculateLineRotation(dxIn, dzIn);
			}
			else {
				// 直角：入ると出るの方向が同じじゃない
				rendererToUse = m_PathCornerRenderer;
				// 入口隣のタイルと出口隣のタイルの相対中心偏移
				int n1x = prevTile->gridX - currTile->gridX;
				int n1z = prevTile->gridZ - currTile->gridZ;
				int n2x = nextTile->gridX - currTile->gridX;
				int n2z = nextTile->gridZ - currTile->gridZ;
				rotY = CalculateCornerRotation(n1x, n1z, n2x, n2z);
			}
		}
		else {
			// ゴールタイル
			// 入るだけ
			// 直線を使って、回転して、前進方向を指す
			int dx = currTile->gridX - prevTile->gridX;
			int dz = currTile->gridZ - prevTile->gridZ;

			rendererToUse = m_PathLineRenderer; 
			rotY = CalculateLineRotation(dx, dz);
		}

		// 描画
		if (rendererToUse) {
			Matrix4x4 world = Matrix4x4::CreateScale(Vector3(1.0f, 1.0f, 1.0f)) 
				* Matrix4x4::CreateRotationY(rotY)
				* Matrix4x4::CreateTranslation(pos);

			Renderer::SetWorldMatrix(&world);
			rendererToUse->Draw();
		}
	}
}

void Player::DrawAttackWarning() {
	//全て攻撃できる方向のヒントUIの描画
	std::vector<Tile*> neighbors;
	int dx[] = { 0, 0, -1, 1 };
	int dz[] = { 1, -1, 0, 0 }; //四方向
	for (int i = 0; i < 4; ++i) {
		Tile* t = m_context->GetMapManager()->GetTile(m_gridX + dx[i], m_gridZ + dz[i]);
		if (t) neighbors.push_back(t);
	}

	Renderer::SetBlendState(BS_ALPHABLEND);
	Renderer::SetDepthEnable(false);
	m_context->GetMapManager()->DrawColoredTiles(neighbors, Color(1.0f, 0.0f, 0.0f, 0.1f)); // ごく薄い赤

	//現在選択したタイル（攻撃方向）を描画
	DirOffset offset = DirOffset::From(m_attackDir);
	Tile* target = m_context->GetMapManager()->GetTile(m_gridX + offset.x, m_gridZ + offset.z);
	if (target) {
		std::vector<Tile*> one{ target };
		m_context->GetMapManager()->DrawColoredTiles(one, Color(1.0f, 0.0f, 0.0f, 0.6f)); // 明るい赤
	}

	Renderer::SetDepthEnable(true);
	Renderer::SetBlendState(BS_NONE);
}

//移動アニメション状態に切り替え
void Player::ExecuteMove() {

	if (m_currentPath.size() < 2) {
		return;
	}
	// gridX,gridZをプレビュー位置に更新
	m_gridX = m_previewGridX;
	m_gridZ = m_previewGridZ;

	// タイルの占有状態を更新
	Tile* oldTile = m_context->GetMapManager()->GetTile(m_startGridX, m_startGridZ);
	if (oldTile) oldTile->occupant = nullptr;
	Tile* newTile = m_context->GetMapManager()->GetTile(m_gridX, m_gridZ);
	if (newTile) newTile->occupant = this;

	// プレイヤーの位置を予想ポイントからスタートポイントにリセット
	m_srt.pos = m_context->GetMapManager()->GetWorldPosition(m_startGridX, m_startGridZ);
	m_pathAnimIndex = 1; // １から始まる、０はスタート位置

	m_state = PlayerState::ANIM_MOVE;
}
void Player::OnTurnChanged(TurnState state)
{
	std::cerr << "player turn OnTurnChanged " << std::endl;
	if(state==TurnState::PlayerPhase)
	{
		//プレイヤーのターン開始
		StartTurn();
	}
	else if(state==TurnState::EnemyPhase)
	{
		//プレイヤーのターン終了
		canControl = false;
	}
}
void Player::StartTurn()
{
	canControl = true;
	ResetMovePoints();
	//移動メニューを使えるとする
	m_hasMoved = false; 
	m_context->GetUIManager()->SetMoveOptionEnabled(true);

	//スタートポイントを保存
	m_startGridX = m_gridX;
	m_startGridZ = m_gridZ;
	m_previewGridX = m_gridX;
	m_previewGridZ = m_gridZ;
	//メインメニューに切り替え
	SwitchToMenuMain();
}
void Player::EndTurn()
{
	canControl = false;
	m_state = PlayerState::WAITING;
	m_context->GetUIManager()->CloseMenu();
	m_context->GetTurnManager()->RequestEndTurn();
}
void Player::TakeDamage(int damage,Unit* attacker){
	Unit::TakeDamage(damage, attacker);
	std::cerr << "you have take damage" << std::endl;
}

Unit* Player::GetTargetInLine(int range)
{
	
	for (int i = 1; i <= range; ++i) 
	{
		int checkX = m_gridX + (DirOffset::From(m_facing).x * i);
		int checkZ = m_gridZ + (DirOffset::From(m_facing).z * i);

		Tile* tile = m_context->GetMapManager()->GetTile(checkX, checkZ);

		if (!tile) break;

		if (tile->occupant != nullptr) {

			Unit* targetUnit = dynamic_cast<Unit*>(tile->occupant);

			if(targetUnit)
			{
				if (targetUnit->GetTeam() == Team::Enemy)
				{
					return targetUnit;
				}
			}

			return nullptr;
		}
		return nullptr;
	}
}


void Player::SetFacing(Direction newDir)
{
	m_facing = newDir;

	float angleOffset = PI;//現在使用のモデルの初期向き補正

	float baseAngle = static_cast<int>(m_facing) * (PI / 2.0f);

	m_targetRot.y = baseAngle + angleOffset;

	m_isRotating = true;
}

// パスでの移動更新
bool Player::UpdatePathMovement(float dt) {
	if (m_currentPath.empty()) return true; //パスは空なら終了

	// 現在のターゲットタイルを取得
	Tile* targetTile = m_currentPath[m_pathAnimIndex];
	Vector3 targetPos = m_context->GetMapManager()->GetWorldPosition(targetTile->gridX, targetTile->gridZ);

	//現在の位置とターゲット位置の差分ベクトル
	Vector3 diff = targetPos - m_srt.pos;
	if (diff.LengthSquared() > 0.001f) {
		// 向きを設置
		m_facing = DirOffset::FromVector(diff.x, diff.z);
	}

	// ベクトルの正規化と距離計算
	Vector3 dir = diff;
	float dist = dir.Length();

	float step = MOVE_SPEED * dt;

	if (dist <= step) {
		// ターゲットに到達たら、目標位置にセット
		m_srt.pos = targetPos;
		m_pathAnimIndex++;// 次のターゲットへ

		// すべてのターゲットに到達したかチェック
		if (m_pathAnimIndex >= m_currentPath.size()) {
			return true; // 移動完了
		}
	}
	else {
		// まだ到達していない場合、進行
		dir.Normalize();
		m_srt.pos += dir * step;
	}

	UpdateWorldMatrix(); 
	return false; // 移動中
}
// メインメニューに切り替え
void Player::SwitchToMenuMain() {
	m_state = PlayerState::MENU_MAIN;
	m_nextState = PlayerState::MENU_MAIN;

	// もし既に移動していたら、移動オプションを無効化
	if (m_hasMoved) {
		m_context->GetUIManager()->SetMoveOptionEnabled(false);
	}
	else {
		m_context->GetUIManager()->SetMoveOptionEnabled(true);
	}
	m_context->GetUIManager()->HideGuideUI();
	m_context->GetUIManager()->OpenMainMenu();

	// プレーヤー位置の誤差修正
	m_srt.pos = m_context->GetMapManager()->GetWorldPosition(m_gridX, m_gridZ);
	UpdateWorldMatrix();
}
// 移動選択状態に切り替え
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

	// 初期化予想モデル位置
	m_previewGridX = m_gridX;
	m_previewGridZ = m_gridZ;

	// 移動範囲タイルの取得(BFS)
	m_moveRangeTiles = m_context->GetMapManager()->GetReachableTiles(m_gridX, m_gridZ, m_currentMovePoints);
	m_currentPath.clear();
}
// 攻撃メニューに切り替え
void Player::SwitchToAttackMenu() {
	m_state = PlayerState::MENU_ATTACK;
	m_nextState = PlayerState::MENU_ATTACK;
	m_context->GetUIManager()->OpenAttackMenu();
	// 移動モード：Escだけ
	m_context->GetUIManager()->ShowGuideUI(
		m_srt.pos,
		false,      // No Arrows
		false,      // No Enter
		true        // Show Esc
	);
}
// 攻撃方向選択状態に切り替え
void Player::SwitchToAttackDirSelect(AttackType type) {
	m_selectedAttackType = type;
	m_state = PlayerState::ATTACK_DIR_SELECT;

	m_context->GetUIManager()->CloseMenu();

	// デフォルトの攻撃方向を現在の向きに設定
	m_attackDir = m_facing;
	// 攻撃方向選択状態：矢印+ Enter + Esc
	m_context->GetUIManager()->ShowGuideUI(
		m_srt.pos,
		true,       // Show Arrows
		true,       // Show Enter
		true        // Show Esc
	);
}

//メニュー操作入力処理
void Player::HandleMenuInput() {
	// 移動していない場合のみ、Jキーで移動選択に切り替え
	if (!m_hasMoved && CDirectInput::GetInstance().CheckKeyBufferTrigger(DIK_J)) {
		m_context->GetUIManager()->TriggerSelectAnim(0);
		m_nextState = PlayerState::MOVE_SELECT;
	}
	else if (CDirectInput::GetInstance().CheckKeyBufferTrigger(DIK_K)) {
		m_context->GetUIManager()->TriggerSelectAnim(1);
		m_nextState = PlayerState::MENU_ATTACK;
	}
	else if (CDirectInput::GetInstance().CheckKeyBufferTrigger(DIK_L)) {
		m_context->GetUIManager()->TriggerSelectAnim(2);
		m_nextState = PlayerState::WAITING;
	}
}
// 移動選択入力処理
void Player::HandleMoveInput(float dt) {
	// ESC: プレーヤー位置をリセットしてメインメニューに戻る
	if (CDirectInput::GetInstance().CheckKeyBufferTrigger(DIK_ESCAPE)) {
		// 移動チュートリアルを非表示
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
		int dx = 0, dz = 0;
		if (CDirectInput::GetInstance().CheckKeyBuffer(DIK_W) || CDirectInput::GetInstance().CheckKeyBuffer(DIK_UP)) 
		  dz = 1;
		else if (CDirectInput::GetInstance().CheckKeyBuffer(DIK_S) || CDirectInput::GetInstance().CheckKeyBuffer(DIK_DOWN)) dz = -1;
		else if (CDirectInput::GetInstance().CheckKeyBuffer(DIK_A) || CDirectInput::GetInstance().CheckKeyBuffer(DIK_LEFT)) dx = -1;
		else if (CDirectInput::GetInstance().CheckKeyBuffer(DIK_D) || CDirectInput::GetInstance().CheckKeyBuffer(DIK_RIGHT)) dx = 1;

		if (dx != 0 || dz != 0) {
			int nextX = m_previewGridX + dx;
			int nextZ = m_previewGridZ + dz;

			// 範囲内かチェック、m_moveRangeTilesの中は有効なタイルが一つがあればTRUE
			//移動できる範囲と予想移動先の検査
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
				m_inputCooldown = 0.15f;
				// ルートの更新：startからpreviewまで
				std::vector<Tile*> path = m_context->GetMapManager()->FindPaths(m_startGridX, m_startGridZ, m_previewGridX, m_previewGridZ);

				m_currentPath.clear();

				// 最初にスタートタイルを追加
				Tile* startTile = m_context->GetMapManager()->GetTile(m_startGridX, m_startGridZ);
				if (startTile) m_currentPath.push_back(startTile);
				// 次に経由タイルを追加
				m_currentPath.insert(m_currentPath.end(), path.begin(), path.end());

				// 最後に目的地タイルを追加
				Tile* destTile = m_context->GetMapManager()->GetTile(m_previewGridX, m_previewGridZ);
				if (destTile) 
				{m_currentPath.push_back(destTile);}

				// 向きの更新
				Vector3 dirVec((float)dx, 0, (float)dz);
				SetFacingFromVector(dirVec);
			}
		}
	}

	// Enter: 移動確認
	if (CDirectInput::GetInstance().CheckKeyBufferTrigger(DIK_RETURN)) {
		m_context->GetUIManager()->HideGuideUI();
		ExecuteMove();
	}
}

void Player::HandleAttackMenuInput() {
	if (CDirectInput::GetInstance().CheckKeyBufferTrigger(DIK_J)) {
		m_context->GetUIManager()->TriggerSelectAnim(0);
		SwitchToAttackDirSelect(AttackType::Normal);
		m_nextState = PlayerState::ATTACK_DIR_SELECT;
	}
	else if (CDirectInput::GetInstance().CheckKeyBufferTrigger(DIK_K)) {
		m_context->GetUIManager()->TriggerSelectAnim(1);
		SwitchToAttackDirSelect(AttackType::Push);
		m_nextState = PlayerState::ATTACK_DIR_SELECT;
	}
	else if (CDirectInput::GetInstance().CheckKeyBufferTrigger(DIK_ESCAPE)) {
		m_context->GetUIManager()->HideGuideUI();
		//攻撃メニューからメインメニューに戻る
		SwitchToMenuMain();
	}
}

void Player::HandleAttackDirInput(float dt) {
	if (CDirectInput::GetInstance().CheckKeyBufferTrigger(DIK_ESCAPE)) {
		SwitchToAttackMenu();
		return;
	}

	if (m_inputCooldown > 0.0f) m_inputCooldown -= dt;
	else {
		// WASDで攻撃方向選択
		if (CDirectInput::GetInstance().CheckKeyBuffer(DIK_W)) m_attackDir = Direction::North;
		else if (CDirectInput::GetInstance().CheckKeyBuffer(DIK_S)) m_attackDir = Direction::South;
		else if (CDirectInput::GetInstance().CheckKeyBuffer(DIK_A)) m_attackDir = Direction::West;
		else if (CDirectInput::GetInstance().CheckKeyBuffer(DIK_D)) m_attackDir = Direction::East;

		// モデルの向きを攻撃方向に更新
		DirOffset offset = DirOffset::From(m_attackDir);
		SetFacingFromVector(Vector3((float)offset.x, 0, (float)offset.z));
	}

	if (CDirectInput::GetInstance().CheckKeyBufferTrigger(DIK_RETURN)) {
		m_context->GetUIManager()->HideGuideUI();
		ExecuteAttack();
	}
}

void Player::ExecuteAttack() {
	// m_attackDirで攻撃を実行
	DirOffset offset = DirOffset::From(m_attackDir);
	int targetX = m_gridX + offset.x;
	int targetZ = m_gridZ + offset.z;

	Tile* targetTile = m_context->GetMapManager()->GetTile(targetX, targetZ);

	// 攻撃アニメーション開始
	Vector3 targetPos = m_context->GetMapManager()->GetWorldPosition(targetX, targetZ);
	StartAttackAnimation(targetPos);

	// ダメージ処理
	if (targetTile && targetTile->occupant && targetTile->occupant != this) {
		// 敵にダメージを与える
		targetTile->occupant->TakeDamage(1, this);
		// もし Push 攻撃なら、押す効果を適用
		if (m_selectedAttackType == AttackType::Push) {
			Enemy* enemy = dynamic_cast<Enemy*>(targetTile->occupant);
			if (enemy) enemy->OnPushed(m_attackDir);
		}
	}

	m_state = PlayerState::ANIM_ATTACK;
}

//プレーヤーモデルの向き更新
void Player::UpdatePaperOrientation() {
	// 突然の変化を防ぐため、lastScaleXで最後の左右向きを記憶
	// 初期化は 1.0f (右向き)
	static float lastScaleX = 1.0f;

	float targetRotY = 0.0f;     // 0 = 正面, π = 背後
	float targetScaleX = 1.0f;   // 1 = 右向き, -1 = 左向き（ミラー）

	//現在の方向を決定
	//攻撃方向選択中は攻撃方向を優先
	Direction currentDir = m_facing;
	if (m_state == PlayerState::ATTACK_DIR_SELECT) {
		currentDir = m_attackDir;
	}

	// 方向に基づいて回転とスケールを設定
	switch (currentDir) {
	case Direction::North: //上向き (+Z) -> 背面
		targetRotY = 3.14159f;     // 180度（背面）
		targetScaleX = lastScaleX; // 左右は維持
		break;

	case Direction::South: // 下向き(-Z) ->正面
		targetRotY = 0.0f;         // 正面
		targetScaleX = lastScaleX; // 左右は維持
		break;

	case Direction::East:  // 右向き (+X) -> 正面 + 右向き
		targetRotY = 0.0f;
		targetScaleX = 1.0f;
		lastScaleX = 1.0f; // 更新臨時データ
		break;

	case Direction::West:  // 左向き (-X) -> 正面 + 左向き（ミラー）
		targetRotY = 0.0f;
		targetScaleX = -1.0f;
		lastScaleX = -1.0f; // 更新臨時データ
		break;
	}

	// 回転の適用
	m_srt.rot.y = targetRotY;

	// スケールの適用
	m_srt.scale.x = targetScaleX;

}

