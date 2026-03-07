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

	// ---  Model Front ---
	{
		std::unique_ptr<CStaticMesh> mesh = std::make_unique<CStaticMesh>();
		mesh->Load("assets/model/character/player_front.obj", "assets/model/character/");
		std::unique_ptr<CStaticMeshRenderer> renderer = std::make_unique<CStaticMeshRenderer>();
		renderer->Init(*mesh);
		MeshManager::RegisterMesh<CStaticMesh>("player_front_mesh", std::move(mesh));
		MeshManager::RegisterMeshRenderer<CStaticMeshRenderer>("player_front_mesh", std::move(renderer));
	}
	// ---  Model Back ---
	{
		std::unique_ptr<CStaticMesh> mesh = std::make_unique<CStaticMesh>();
		mesh->Load("assets/model/character/player_back.obj", "assets/model/character/");
		std::unique_ptr<CStaticMeshRenderer> renderer = std::make_unique<CStaticMeshRenderer>();
		renderer->Init(*mesh);
		MeshManager::RegisterMesh<CStaticMesh>("player_back_mesh", std::move(mesh));
		MeshManager::RegisterMeshRenderer<CStaticMeshRenderer>("player_back_mesh", std::move(renderer));
	}



	m_PlayerShader = MeshManager::getShader<CShader>("unlightshader");
	m_PathLineRenderer = MeshManager::getRenderer<CStaticMeshRenderer>("arrow_straight_mesh");
	m_PathCornerRenderer = MeshManager::getRenderer<CStaticMeshRenderer>("arrow_corner_mesh");


	// 基底クラスのレンダラー初期化
	auto* frontR = MeshManager::getRenderer<CStaticMeshRenderer>("player_front_mesh");
	auto* backR = MeshManager::getRenderer<CStaticMeshRenderer>("player_back_mesh");

	// Unitクラスへ制御権を委譲し、前後両面のモデルを登録
	SetModelRenderers(frontR, backR);


	if (m_PlayerShader) {
		std::cerr << "Player Shader Loaded Successfully." << std::endl;
	}
	else {
		std::cerr << "CRITICAL ERROR: 'lightshaderSpecular' not found! Check shader file paths." << std::endl;
	}

	printf("Init LineRenderer = %p\n", m_PathLineRenderer);
}

void Player::Init() {

	playerResourceLoader();

	m_srt.scale = Vector3(1.0f, 1.0f, 1.0f);
	m_srt.rot = Vector3(0, 0, 0);

	m_state = PlayerState::WAITING;
	m_targetWorldPos = m_srt.pos;
	m_moveSpeed = 5.0f;

	SetFacing(Direction::South);
	m_srt.rot.y = m_targetRot.y;
	 

	m_maxMovePoints = 4;
	m_currentMovePoints = m_maxMovePoints;
	m_maxHP = 4;
	m_currentHP = m_maxHP;

	m_hasActioned = false;
	m_isRotating = false;

	m_team = Team::Player;

	UpdateWorldMatrix();
}

void Player::Update(uint64_t dt) {
	//ミリ秒を秒に
	float deltaSeconds = static_cast<float>(dt) / 1000.0f;
	if (m_context->GetUIManager()->IsAnimating()) {return;}

	// UIアニメーションが終わり、プレイヤーのUpdateが再開された最初のフレームでメニューを開く
	if (m_isWaitingTurnStart) {
		Ally* ally = m_context->GetAlly();
		// もし仲間が存在し、かつ脱出アニメーション（採掘やフェード）の最中であれば
		if (ally && ally->IsEscaping()) {
			// ターンする
			// この間、プレイヤーの状態は WAITING のまま維持され、入力は受け付けず、
			// カメラも仲間を注視し続ける
			return;
		}
		m_isWaitingTurnStart = false;
		SwitchToMenuMain();
	}

	// 【フェーズ2】：UIアニメーション終了後、最初のUpdateにてカメラのズームイン（接近）を開始
	if (!m_isZoomedIn) {
		m_isZoomedIn = true;
		if (m_context && m_context->GetCamera()) {
			m_context->GetCamera()->SetTargetRadius(Camera::ZOOM_RADIUS); // ズームイン（接近）
		}
	}

	// 基底クラスの反転（フリップ）アニメーション処理を実行
	UpdateFlipAnimation(deltaSeconds);

	//アニメション終わった後、またメニュー遷移実際に実行する
	//メインメニュー状態の場合
	if (m_state == PlayerState::MENU_MAIN && m_nextState != PlayerState::MENU_MAIN) {
		if (m_nextState == PlayerState::MOVE_SELECT) {
			SwitchToMoveSelect();
		}
		else if (m_nextState == PlayerState::ATTACK_DIR_SELECT) {
			SwitchToAttackDirSelect(AttackType::Push);
		}
		else if (m_nextState == PlayerState::WAITING) {
			EndTurn();
		}
		//安全おため、もう一回リセット
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

		// 【追跡】：移動アニメーション中、プレイヤーを継続的に追従する
		if (m_context && m_context->GetCamera()->GetState() == CameraState::Tracking) {
			m_context->GetCamera()->SetTargetLookAt(m_srt.pos);
		}

		if (UpdatePathMovement(deltaSeconds)) {
			
			m_hasMoved = true; //移動完了のフラグを立てる

			// 現在位置のタイルのイベント（トラップ等）を発火させる
			// アニメーション中の現在ステップに対応するタイルを取得
			Tile* finalTile = m_context->GetMapManager()->GetTile(m_gridX, m_gridZ);

			// タイルが存在し、かつその上に構造物（Trapなど）が配置されているかチェック
			if (finalTile && finalTile->structure) {
				// プレイヤーがオブジェクトを踏んだ（進入した）際のイベントを実行
				// ※ this（Player自身）を引数として渡し、ダメージ処理などを行わせる
				finalTile->structure->OnEnter(this);
			}

			//メインメニューに切り替え
			SwitchToMenuMain();
		}
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
	case PlayerState::KNOCKBACK:
		// スライディング移動（ノックバック）先がある場合
		if (m_slideEndPos.LengthSquared() > 0.001f) {
			if (UpdateSlideAnimation(dt)) { // アニメーション更新（ミリ秒dtを渡す）
				m_slideEndPos = Vector3(0, 0, 0); // 到着したので目標座標をリセット

				// 押し出された先のタイルにギミック（罠など）があるかチェック
				Tile* t = m_context->GetMapManager()->GetTile(m_gridX, m_gridZ);
				if (t && t->structure) t->structure->OnEnter(this);

				// 操作可能な状態ならメインメニューへ、そうでなければ待機状態へ
				if (canControl) SwitchToMenuMain();
				else m_state = PlayerState::WAITING;
			}
		}
		else {
			// 壁に衝突して移動が発生しなかった場合
			if (canControl) SwitchToMenuMain();
			else m_state = PlayerState::WAITING;
		}
		break;
	case PlayerState::ANIM_CELEBRATE:
		UpdateCelebration(deltaSeconds);
		break;
	case PlayerState::WAITING:
		// プレーヤーターン以外の待機状態
		break;
	
	}
	//向きの更新
	/*UpdatePaperOrientation();*/

	UpdateWorldMatrix();
}

// 1. エンティティレイヤー (5.1)
void Player::OnDraw(uint64_t dt) {
	Renderer::SetPixelArtMode(true);
	if (m_PlayerShader != nullptr) m_PlayerShader->SetGPU();
	DrawModel(); // 3Dモデルの描画のみを行う
	Renderer::SetPixelArtMode(false);
}

// 2. 床面 UI レイヤー (3)
void Player::OnDrawFloorUI(uint64_t dt) {
	if (m_PlayerShader != nullptr) m_PlayerShader->SetGPU();

	if (m_state == PlayerState::MOVE_SELECT) {
		// 移動範囲タイルの描画
		m_context->GetMapManager()->DrawColoredTiles(m_moveRangeTiles, Color(0, 1, 0, 0.4f));
		DrawPathLine(); // ルート矢印
	}
	else if (m_state == PlayerState::ATTACK_DIR_SELECT) {
		DrawAttackWarningFloor(); // 赤い警告エリア（床面）
	}
}

// 3. 半透明エンティティレイヤー (5.3)
void Player::OnDrawTransparent(uint64_t dt) {
	if (m_state == PlayerState::MOVE_SELECT) {
		// ゴースト（残像）の描画
		// ※ 内部ロジックからも Renderer::SetBlendState 等のステート操作を削除し、呼び出し側に一任する
		DrawGhost();
	}
}

// 4. 浮遊 Overlay レイヤー (6)
void Player::OnDrawOverlay(uint64_t dt) {
	if (m_state == PlayerState::ATTACK_DIR_SELECT) {
		// 攻撃プレビュー（敵のノックバック予測を含む最前面UI）を表示
		DrawAttackWarningOverlay();
	}
}

//スタート点の半透明残影
void Player::DrawGhost() {

	Vector3 ghostPos = m_context->GetMapManager()->GetWorldPosition(m_startGridX, m_startGridZ);
	ghostPos.y += 0.015f;

	Matrix4x4 ghostWorld = Matrix4x4::CreateScale(m_srt.scale) * Matrix4x4::CreateRotationY(m_srt.rot.y) * Matrix4x4::CreateTranslation(ghostPos);

	Renderer::SetWorldMatrix(&ghostWorld);

	// マテリアルを灰色半透明に変更して描画
	CMaterial* mtrl = m_frontRenderer->GetMaterial(0);
	MATERIAL old = mtrl->GetData();
	MATERIAL gray = old;
	gray.Diffuse = Color(1.0f, 1.0f, 1.0f, 0.6f); //灰色半透明
	mtrl->SetMaterial(gray);

	m_frontRenderer->Draw();

	MATERIAL restore = old;
	restore.Diffuse = Color(1.0f, 1.0f, 1.0f, 1.0f);
	restore.Ambient = Color(1.0f, 1.0f, 1.0f, 1.0f); 
	restore.Emission = Color(0.1f, 0.1f, 0.1f, 1.0f); 
	restore.TextureEnable = TRUE;

	mtrl->SetMaterial(restore);

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
	// スタートポイントは描画しない
	if (m_currentPath.empty()) return;

	// 目的地が危険（トラップ）かどうかをチェック
	bool isDanger = false;
	if (!m_currentPath.empty()) {
		Tile* destTile = m_currentPath.back();
		if (destTile && destTile->structure && destTile->structure->GetType() == MapModelType::TRAP) {
			// 【修正】Trap ポインタにキャストし、既に発動済みかどうかを判定
			Trap* trap = dynamic_cast<Trap*>(destTile->structure);
			if (trap && !trap->IsActivated()) {
				isDanger = true; // 未発動のトラップのみ、赤色の危険ルートとして表示する
			}
		}
	}

	// カラー定義
	Color normalColor(1.0f, 1.0f, 1.0f, 1.0f); // 白色（通常時）
	Color dangerColor(1.0f, 0.0f, 0.0f, 1.0f); // 赤色（危険時）
	Color targetColor = isDanger ? dangerColor : normalColor;

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
		pos.y += 0.06f; //地面よりちょっと高い

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
			// 一時的なマテリアル色の変更
			if (auto* mat = rendererToUse->GetMaterial(0)) {
				MATERIAL original = mat->GetData();
				MATERIAL temp = original;
				temp.Diffuse = targetColor; // 色を適用（白または赤）
				mat->SetMaterial(temp);

				rendererToUse->Draw();

				// マテリアルを復元 (次のフレームで再設定されますが、状態をクリーンに保つための良い習慣です)
				mat->SetMaterial(original);
			}
			else {
				// マテリアルが取得できない場合は通常通り描画
				rendererToUse->Draw();
			}

		}
	}
}

void Player::DrawAttackWarningFloor() {
	//全て攻撃できる方向のヒントUIの描画
	std::vector<Tile*> neighbors;
	int dx[] = { 0, 0, -1, 1 };
	int dz[] = { 1, -1, 0, 0 };//四方向
	for (int i = 0; i < 4; ++i) {
		Tile* t = m_context->GetMapManager()->GetTile(m_gridX + dx[i], m_gridZ + dz[i]);
		if (t) neighbors.push_back(t);
	}
	m_context->GetMapManager()->DrawColoredTiles(neighbors, Color(0.9f, 0.0f, 0.0f, 0.2f));
	//現在選択したタイル（攻撃方向）を描画
	DirOffset offset = DirOffset::From(m_attackDir);
	Tile* target = m_context->GetMapManager()->GetTile(m_gridX + offset.x, m_gridZ + offset.z);
	if (target) {
		std::vector<Tile*> one{ target };
		m_context->GetMapManager()->DrawColoredTiles(one, Color(0.9f, 0.0f, 0.0f, 0.8f));
	}
}

void Player::DrawAttackWarningOverlay() {
	DirOffset offset = DirOffset::From(m_attackDir);
	Tile* target = m_context->GetMapManager()->GetTile(m_gridX + offset.x, m_gridZ + offset.z);
	// === ノックバックUIプレビューをトリガー ===
	// Push（押し出し）を選択中、かつターゲットのマスに自分以外のユニット（敵または味方）がいる場合、押し出しのプレビューを描画する
	if (target && m_selectedAttackType == AttackType::Push && target->occupant && target->occupant != this) {
		// ターゲットのユニットが持つプレビュー描画メソッドを直接呼び出す
		target->occupant->DrawPushPreview(m_attackDir);
	}
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
	
	// ターン開始を記録し、フェーズ2のズームイン（拡大）実行を準備
	m_isZoomedIn = false;

	//スタートポイントを保存
	m_startGridX = m_gridX;
	m_startGridZ = m_gridZ;
	m_previewGridX = m_gridX;
	m_previewGridZ = m_gridZ;
	// すぐに SwitchToMenuMain() を呼ばず、アニメーション待ち状態にする
	m_state = PlayerState::WAITING;
	m_isWaitingTurnStart = true;
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
					return targetUnit;
			}

			return nullptr;
		}

	}
	return nullptr;
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
		// Unit::SetFacing が呼ばれ、紙片反転アニメーションとモデル切り替えが発動します。
		SetFacingFromVector(diff);
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

	// 【カメラ帰還】：メインメニューに戻る際、プレイヤーの追跡を再開
	if (m_context && m_context->GetCamera()) {
		m_context->GetCamera()->SetState(CameraState::Tracking);
		m_context->GetCamera()->SetTargetLookAt(m_srt.pos);
	}

	// もし既に移動していたら、移動オプションを無効化
	if (m_hasMoved) {
		m_context->GetUIManager()->SetMoveOptionEnabled(false);
	}
	else {
		m_context->GetUIManager()->SetMoveOptionEnabled(true);
	}

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
	// 【戦闘カメラ演出】：攻撃方向の選択時、カメラを攻撃方向へ少し前進（オフセット）させる
	if (m_context && m_context->GetCamera()) {
		m_context->GetCamera()->SetState(CameraState::ActionFocus);
		DirOffset offset = DirOffset::From(m_attackDir);
		// 攻撃方向へ 1.5 マス分オフセットさせた位置をターゲットにする
		Vector3 targetPos = m_srt.pos + Vector3((float)offset.x, 0.0f, (float)offset.z) * 1.5f;
		m_context->GetCamera()->SetTargetLookAt(targetPos);
	}
}

//メニュー操作入力処理
void Player::HandleMenuInput() {
	// 移動していない場合のみ、Jキーで移動選択に切り替え
	if (!m_hasMoved && CDirectInput::GetInstance().CheckKeyBufferTrigger(DIK_J)) {
		m_context->GetUIManager()->TriggerSelectAnim(0);
		m_nextState = PlayerState::MOVE_SELECT;
	}
	// 【変更】攻撃可能な場合のみKキーの入力を受け付ける
	else if (m_canAttack && CDirectInput::GetInstance().CheckKeyBufferTrigger(DIK_K)) {
		m_context->GetUIManager()->TriggerSelectAnim(1);
		m_nextState = PlayerState::ATTACK_DIR_SELECT;
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
		// 1. まず、プレイヤーの「絶対スクリーン入力」 (スクリーン空間入力) を取得する
		int inX = 0, inZ = 0;
		if (CDirectInput::GetInstance().CheckKeyBuffer(DIK_W) || CDirectInput::GetInstance().CheckKeyBuffer(DIK_UP)) inZ = 1;
		else if (CDirectInput::GetInstance().CheckKeyBuffer(DIK_S) || CDirectInput::GetInstance().CheckKeyBuffer(DIK_DOWN)) inZ = -1;
		else if (CDirectInput::GetInstance().CheckKeyBuffer(DIK_A) || CDirectInput::GetInstance().CheckKeyBuffer(DIK_LEFT)) inX = -1;
		else if (CDirectInput::GetInstance().CheckKeyBuffer(DIK_D) || CDirectInput::GetInstance().CheckKeyBuffer(DIK_RIGHT)) inX = 1;

		if (inX != 0 || inZ != 0) {

			// 2. カメラの現在の正規化されたインデックス (0, 1, 2, 3) を取得
			int camDir = 0;
			if (m_context && m_context->GetCamera()) {
				camDir = m_context->GetCamera()->GetNormalizedDirIndex();
			}

			// 3. カメラの位置（向き）に応じて入力ベクトルを回転させる
			// 90度回転させるごとの座標変換ルール：新しい x = 旧 z、新しい z = -旧 x
			// (カメラが時計回りか反時計回りかによって正負が異なるため、
			// 挙動が逆の場合は下記の inX と inZ の符号を入れ替えて調整すること)
			int dx = 0, dz = 0;
			switch (camDir) {
			case 0:
				// インデックス 0：正面左 (基本カメラ位置) 
				dx = inX;
				dz = inZ;
				break;
			case 1:
				// インデックス 1：正面右 
				dx = -inZ;
				dz = inX;
				break;
			case 2:
				// インデックス 2：背面右
				dx = -inX;
				dz = -inZ;
				break;
			case 3:
				// インデックス 3：背面左 
				dx = inZ;
				dz = -inX;
				break;
			}

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
				std::vector<Tile*> path = m_context->GetMapManager()->FindPaths(m_startGridX, m_startGridZ, m_previewGridX, m_previewGridZ,true);

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

				// 【カーソル移動】：カメラの目標注視点をカーソルのプレビュー位置に更新
				Vector3 previewPos = m_context->GetMapManager()->GetWorldPosition(m_previewGridX, m_previewGridZ);
				m_context->GetCamera()->SetTargetLookAt(previewPos);
			}
		}
	}

	// Enter: 移動確認
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
		// 1. プレイヤーの「スクリーン空間入力」を取得する
		int inX = 0, inZ = 0;
		if (CDirectInput::GetInstance().CheckKeyBuffer(DIK_W) || CDirectInput::GetInstance().CheckKeyBuffer(DIK_UP)) inZ = 1;
		else if (CDirectInput::GetInstance().CheckKeyBuffer(DIK_S) || CDirectInput::GetInstance().CheckKeyBuffer(DIK_DOWN)) inZ = -1;
		else if (CDirectInput::GetInstance().CheckKeyBuffer(DIK_A) || CDirectInput::GetInstance().CheckKeyBuffer(DIK_LEFT)) inX = -1;
		else if (CDirectInput::GetInstance().CheckKeyBuffer(DIK_D) || CDirectInput::GetInstance().CheckKeyBuffer(DIK_RIGHT)) inX = 1;

		if (inX != 0 || inZ != 0) {
			// 2. カメラの正規化されたインデックスを取得
			int camDir = 0;
			if (m_context && m_context->GetCamera()) {
				camDir = m_context->GetCamera()->GetNormalizedDirIndex();
			}

			// 3. カメラの向きに応じて入力ベクトルを回転させる
			int dx = 0, dz = 0;
			switch (camDir) {
			case 0:
				// インデックス 0：正面左 (基本カメラ位置)
				dx = inX;
				dz = inZ;
				break;
			case 1:
				// インデックス 1：正面右 
				dx = -inZ;
				dz = inX;
				break;
			case 2:
				// インデックス 2：背面右
				dx = -inX;
				dz = -inZ;
				break;
			case 3:
				// インデックス 3：背面左 
				dx = inZ;
				dz = -inX;
				break;
			}

			// 4. 回転後のワールド座標系ベクトルを Direction 列挙型にマッピングする
			if (dz == 1) m_attackDir = Direction::North;
			else if (dz == -1) m_attackDir = Direction::South;
			else if (dx == -1) m_attackDir = Direction::West;
			else if (dx == 1) m_attackDir = Direction::East;

			// モデルの向きを攻撃方向に更新
			DirOffset offset = DirOffset::From(m_attackDir);
			SetFacingFromVector(Vector3((float)offset.x, 0, (float)offset.z));

			// 【戦闘カメラの更新】：方向変更に合わせてカメラのオフセットを更新
			if (m_context && m_context->GetCamera()) {
				DirOffset offset = DirOffset::From(m_attackDir);
				Vector3 targetPos = m_srt.pos + Vector3((float)offset.x, 0.0f, (float)offset.z) * 2.5f;
				m_context->GetCamera()->SetTargetLookAt(targetPos);
			}
		}
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
			targetTile->occupant->OnPushed(m_attackDir);
		}
	}

	m_state = PlayerState::ANIM_ATTACK;
}

// 勝利アニメーション開始
void Player::StartCelebration() {
	// 勝利アニメーションの初期化
	m_state = PlayerState::ANIM_CELEBRATE;
	m_jumpCount = 0;
	m_jumpTimer = 0.0f;
	m_isCelebrationDone = false;
	m_context->GetUIManager()->CloseMenu(); // メニューを閉じる
	SetFacing(Direction::South); // 勝利ポーズの向きに設定
}
// 勝利アニメーションの更新
void Player::UpdateCelebration(float dt) {
	// ジャンプの高さをサイン波で変化させるためのパラメータ
	float jumpSpeed = 15.0f;
	float jumpHeight = 0.5f;
	// 前回のサイン値と今回のサイン値を比較して、ジャンプの上下動を実現
	float previousSin = sinf(m_jumpTimer * jumpSpeed);
	m_jumpTimer += dt;
	float currentSin = sinf(m_jumpTimer * jumpSpeed);

	// Y軸のジャンプオフセットを計算 (絶対値によりバウンドを実現)
	//fabsfを使って、サイン波の上下動を正の値に変換し、ジャンプの高さを調整
	float yOffset = fabsf(currentSin) * jumpHeight;

	// 地面の基本高度を取得
	Vector3 basePos = m_context->GetMapManager()->GetWorldPosition(m_gridX, m_gridZ);
	m_srt.pos.y = basePos.y + yOffset;

	// 着地の判定 (sin値が正から負に変わる瞬間を一回のジャンプ完了とみなす)
	if (previousSin >= 0.0f && currentSin < 0.0f) {
		m_jumpCount++;
	}

	if (m_jumpCount >= 6) {
		m_srt.pos.y = basePos.y; // 確実に接地させる
		m_isCelebrationDone = true;
	}
}

void Player::OnPushed(Direction pushDir) {
	if (m_currentHP <= 0) return;
	m_state = PlayerState::KNOCKBACK;
	if (m_context && m_context->GetUIManager()) m_context->GetUIManager()->CloseMenu();
	Unit::OnPushed(pushDir);// 基底クラスの処理を呼び出し、グリッド移動・衝突判定・アニメーション開始を実行
}