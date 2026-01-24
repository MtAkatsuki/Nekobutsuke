#include "../scene/GameScene.h"
#include "../system/CDirectInput.h"
#include "../utility/ScreenToWorld.h"
#include"../utility/WorldToScreen.h"
#include "../system/SphereDrawer.h"
#include "../system/DebugUI.h"
#include "../system/CPolar3D.h"
#include "../system/meshmanager.h"
#include "../manager/MapManager.h"
#include "../manager/GameContext.h"
#include	"../ui/GameUIManager.h"
#include "../ui/DamageNumberManager.h"
#include "../ui/DialogueUI.h"
#include "../system/scenemanager.h"
#include "../system//FadeTransition.h"
#include <stdio.h> // for sprintf_s
#include <iostream>


namespace {
}

/**
 * @brief コンストラクタ
 */
GameScene::GameScene()
{

}

/**
 * @brief シーンの初期化処理
 */
void GameScene::Init()
{
	// 二重の安全策：入場時に全マネージャーを強制クリーンアップ
	// dispose処理の未完了や残留データによる不具合を防止
	// =========================================================
	if (m_context) {
		// ターン関連のコールバックをクリア（前ゲームのゾンビコールバックによるクラッシュ防止）
		if (m_context->GetTurnManager()) {
			std::cerr << "   [Init] Clearing Turn Observers..." << std::endl;
			m_context->GetTurnManager()->ClearObservers();
		}
		// 敵リストのクリア
		if (m_context->GetEnemyManager()) {
			m_context->GetEnemyManager()->ClearAll();
		}
		// UIの初期化
		if (m_context->GetUIManager()) {
			m_context->GetUIManager()->Clear();
		}
		// 前ゲームの参照を遮断（野ポインタ/ダングリングポインタ防止）
		m_context->SetPlayer(nullptr);
		m_context->SetAlly(nullptr);
	}
	// =========================================================
	std::cerr << "=== GameScene::Init Start ===" << std::endl;
	// カメラ(3D)の初期化

	float azimuth = 1.58f;
	float elevation = -1.08f;
	float radius = 9.65f;
	Vector3 lookat(0, 0, 0);

	CPolor3D polor(radius, elevation, azimuth);
	Vector3 cpos = polor.ToCartesian();
	//位置計算
	CPolor3D polorup(1.0f, elevation + PI / 2.0f, azimuth);
	Vector3 upvector = polorup.ToCartesian();
	//上方のベクトルを計算
	std::cerr << "   [GameScene] Creating Camera..." << std::endl;
	m_camera = m_context->GetCamera();
	m_camera->SetPosition(cpos + lookat);
	m_camera->SetLookat(lookat);
	m_camera->SetUP(upvector);


	// リソースを読み込む
	std::cerr << "Loading Resources..." << std::endl;
	resourceLoader();
	std::cerr << "   [GameScene] Resources Loaded." << std::endl;

	std::cerr << "   [GameScene] Getting Shader..." << std::endl;
	m_tileShader = MeshManager::getShader<CShader>("unlightshader");
	if (m_tileShader == nullptr) {
		std::cerr << "   [FATAL] Shader 'lightshaderSpecular' is NULL! Check resourceLoader." << std::endl;
	}
	else {
		std::cerr << "   [GameScene] Shader OK." << std::endl;
	}

	// MapManager初期化
	if (m_context == nullptr) {
		std::cerr << "   [FATAL] m_context is NULL! Did SceneManager pass it?" << std::endl;
	}
	m_MapManager = m_context->GetMapManager();
	m_MapManager->SetScene(this);
	m_MapManager->Init(m_context);
	std::cerr << "   [GameScene] MapManager OK." << std::endl;
	// 背景
	m_background = std::make_unique<Background>();
	m_background->Init();
	std::cerr << "   [GameScene] Background OK" << std::endl;

	// プレイヤ
	auto playerPtr = std::make_unique<Player>(m_context);
	playerPtr->Init();
	m_player = playerPtr.get();
	m_context->SetPlayer(m_player);
	AddObject(std::move(playerPtr));
	std::cerr << "   [GameScene] Player OK." << std::endl;
	// Ally初期化
	auto m_allyPtr = std::make_unique<Ally>(m_context);
	m_allyPtr->Init();
	m_ally = m_allyPtr.get();
	//Allyの固定出現場所
	int allyTargetX = 1;
	int allyTargetZ = 4;

	Tile* allyTile = m_MapManager->GetTile(allyTargetX, allyTargetZ);
	if (allyTile) {
		allyTile->occupant = m_ally;
		m_ally->setPosition(m_MapManager->GetWorldPosition(allyTargetX, allyTargetZ));
		m_ally->UpdateWorldMatrix();
		m_ally->SetGridPosition(allyTargetX, allyTargetZ);
	}
	m_context->SetAlly(m_ally);
	AddObject(std::move(m_allyPtr));
	std::cerr << "   [GameScene] Ally OK." << std::endl;

	// 敵
	for (int i = 0; i < ENEMYMAX; ++i) {
		auto enemyPtr = std::make_unique<Enemy>(m_context);

		m_enemies[i] = enemyPtr.get();

		m_enemies[i]->Init(i);

		m_context->GetEnemyManager()->RegisterEnemy(m_enemies[i]);

		AddObject(std::move(enemyPtr));
	}
	std::cerr << "   [GameScene] Enemies OK." << std::endl;

	m_gameUIManager = m_context->GetUIManager();


	//UI_cutin
	m_turnCutin = std::make_unique<TurnCutin>();
	m_turnCutin->Init();

	m_context->GetTurnManager()->RegisterObserver([this](TurnState state) {
		if (state == TurnState::PlayerPhase) {
			m_turnCutin->PlayCutinAnimation("Player Phase");
		}
		else if (state == TurnState::EnemyPhase) {
			m_turnCutin->PlayCutinAnimation("Enemy Phase");

			m_context->GetEnemyManager()->StartEnemyPhase();
		}
		});
	//勝利条件のカウンター
	m_turnCounter = std::make_unique<TurnCounter>();
	m_turnCounter->Init();
	// 吹き出しUI取得
	m_dialogueUI = m_context->GetDialogueUI();
	// ダメージ数字管理取得
	m_damageNumberManager = m_context->GetDamageManager();
	//UI管理取得
	m_gameUIManager = m_context->GetUIManager();

	std::cerr << "   [GameScene] Registering DebugUI..." << std::endl;
	//DebugUI::RedistDebugFunction([this]() {
	//	debugUICamera();
	//	});

	//DebugUI::RedistDebugFunction([this]() {
	//	drawGridDebugText();
	//	});



	//std::cerr << "   [GameScene] Starting Turn..." << std::endl;
	//if (m_context->GetTurnManager()) {
	//	m_context->GetTurnManager()->SetState(TurnState::PlayerPhase);
	//}

	//std::cerr << "   [GameScene] Init Finished Success!" << std::endl;

	m_isGameStarted = false;
	m_startDelayTimer = 0.0f;

	m_isSceneChanging = false;
}


void GameScene::debugUICamera() {
	if (!m_enableDebugCamera) return;

	ImGui::Begin("Debug Camera");


	static const char* mode_names[] = { "Orbit", "Free" };
	static int mode_index = static_cast<int>(m_cameraDebugMode);
	if (ImGui::Combo("Camera Mode", &mode_index, mode_names, IM_ARRAYSIZE(mode_names))) {
		m_cameraDebugMode = static_cast<CameraDebugMode>(mode_index);
	}

	Vector3 lookatpos = m_camera->GetLookat(); 

	if (m_cameraDebugMode == CameraDebugMode::Orbit) {
		static float azimuth = 1.58f;
		static float elevation = -1.08f;
		static float radius = 9.65f;

		ImGui::DragFloat("Azimuth", &azimuth, 0.01f, -PI, PI, "%.4f");
		ImGui::DragFloat("Elevation", &elevation, 0.01f, -PI / 2.0f + 0.01f, PI / 2.0f - 0.01f, "%.4f");
		ImGui::DragFloat("Radius", &radius, 0.1f, 0.5f, 25.0f, "%.3f");

		
		CPolor3D polor(radius, elevation, azimuth);
		Vector3 cpos = polor.ToCartesian();

		CPolor3D polorup(1.0f, elevation + PI / 2.0f, azimuth);
		Vector3 upvector = polorup.ToCartesian();

		m_camera->SetPosition(cpos + lookatpos);
		m_camera->SetLookat(lookatpos);
		m_camera->SetUP(upvector);
	}

	else if (m_cameraDebugMode == CameraDebugMode::Free) {
		static Vector3 freePos = m_camera->GetPosition();
		static Vector3 freeUp = m_camera->GetUP();
		static Vector3 freeLookAt = lookatpos; 

	
		if (ImGui::IsWindowAppearing()) {
			freePos = m_camera->GetPosition();
			freeUp = m_camera->GetUP();
			freeLookAt = m_camera->GetLookat();
		}

		ImGui::DragFloat3("Position", &freePos.x, 0.5f, -100.0f, 100.0f, "%.2f");
		ImGui::DragFloat3("Look At", &freeLookAt.x, 0.5f, -100.0f, 100.0f, "%.2f");
		ImGui::DragFloat3("Up", &freeUp.x, 0.01f, -1.0f, 1.0f, "%.3f");


		m_camera->SetPosition(freePos);
		m_camera->SetLookat(freeLookAt);
		m_camera->SetUP(freeUp);
	}

	
	ImGui::Separator();
	ImGui::Separator();

	ImGui::SliderFloat3("Near Point", &m_nearpoint.x, -10000.0f, 10000.0f);
	ImGui::SliderFloat3("Far Point", &m_farpoint.x, -10000.0f, 10000.0f);
	ImGui::SliderFloat3("Pickup Pos", &m_pickuppos.x, -10000.0f, 10000.0f);

	ImGui::End();
}


/**
 * @brief シーンの更新処理
 *
 * @param deltatime 前フレームからの経過時間（ミリ秒）
 */
void GameScene::update(uint64_t deltatime)
{	
	float deltaSeconds = static_cast<float>(deltatime) / 1000.0f;

	if (!m_isGameStarted)
	{
		m_startDelayTimer += deltaSeconds;

		// FadeInを一秒待ち
		if (m_startDelayTimer >= START_WAIT_TIME)
		{
			m_isGameStarted = true;

			//PlayerTurnアニメーション始まる
			if (m_context->GetTurnManager()) {
				std::cout << "[GameScene] FadeIn Complete. Start Player Phase!" << std::endl;
				m_context->GetTurnManager()->SetState(TurnState::PlayerPhase);
			}
		}

		// 待つ時、背景だけアップデート
		if (m_background) m_background->Update(deltatime);
		return;
	}
	//背景の更新
	if (m_background) m_background->Update(deltatime);

	//TurnAnimation進行中は他の更新処理をスキップ
	if (m_turnCutin && m_turnCutin->IsAnimating()) {
		m_turnCutin->Update(deltatime);
		return;
	}
	// UIの更新
	if (m_gameUIManager) {
		m_gameUIManager->Update(deltatime);
	}
	// ゲームオブジェクトの更新（アニメション、エフェクトなど）
	for (const auto& obj : m_GameObjectList) {
		obj->Update(deltatime);
	}

	//ダメージ数字の更新
	if (m_damageNumberManager) {
		m_damageNumberManager->Update(deltatime);
	}

	if (m_context->GetTurnManager()->GetTurnState() == TurnState::PlayerPhase) {
		if (m_turnCutin && !m_turnCutin->IsAnimating()) {
			if (!m_isAllyTalked) {

				// Allyが生存している場合のみ表示
				if (m_ally && m_ally->GetHP() > 0) {

					// Allyの位置を取得
					Vector3 allyPos = m_ally->getSRT().pos;

					// 「吹き出し」を表示
					m_dialogueUI->ShowDialogue(allyPos);
				}

				m_isAllyTalked = true;
			}
		}
	}

	if (m_context->GetDialogueUI()) {
		m_context->GetDialogueUI()->Update(deltatime);
	}

	//敵の更新
	m_context->GetEnemyManager()->Update(deltatime);
	//ターン変更チェック及び処理
	TurnChangeCheck();

	//クリアチェック及びシーン遷移
	CheckGameStatus(deltaSeconds);

}

/**
 * @brief 描画処理
 *
 * @param deltatime 前フレームからの経過時間（ミリ秒）
 */
void GameScene::draw(uint64_t deltatime)
{
	//背景の描画
	// Zバッファを無効化(Backgroundが最背面に描画されるようにする)
	Renderer::SetDepthEnable(false);

	if (m_background) {
		m_background->Draw();
	}

	Renderer::SetDepthEnable(true);

	m_camera->Draw();

	if (m_tileShader != nullptr){ 
		m_tileShader->SetGPU(); }
	

	for (const auto& obj : m_GameObjectList) {
		obj->Draw(deltatime);
	}
	//2D描画処理
	//Zバッファを無効化し、ブレンディングを有効化することで、UIが最前面に描画されるようにする
	Renderer::SetDepthEnable(false);//Zバッファ無効化
	Renderer::SetBlendState(BS_ALPHABLEND);//混色

	if (m_tileShader) {
		m_tileShader->SetGPU();
	}

	// プレーヤー HPの描画
	if (m_player) {
		m_player->DrawUI();
	}
	//味方　HPの描画
	if (m_ally && m_ally->GetHP() > 0) {
		m_ally->DrawUI();
	}
	// エネミー HPの描画
	for (int i = 0; i < ENEMYMAX; ++i) {
		// 生きている敵のみ描画
		if (m_enemies[i] && !m_enemies[i]->IsDead()) {
			m_enemies[i]->DrawUI();
		}
	}

	if (m_damageNumberManager) {
		m_damageNumberManager->Draw();
	}

	if (m_gameUIManager) {
		m_gameUIManager->Draw();
	}

	if (m_turnCutin) {
		m_turnCutin->Draw();
	}

	if (m_dialogueUI) {
		m_dialogueUI->Draw();
	}

	if (m_turnCounter) m_turnCounter->Draw();

	Renderer::SetDepthEnable(true);
}



/**
 * @brief シーンの終了処理
 */
void GameScene::dispose()
{
	//ダメージ数字をクリアする
	if (m_context && m_damageNumberManager) {
		m_damageNumberManager->ClearAll();
	}


	//TurnManagerの観察者をクリアする
	if (m_context && m_context->GetTurnManager()) {
		m_context->GetTurnManager()->ClearObservers();
	}

	//EnemyManagerをクリアする
	if (m_context && m_context->GetEnemyManager()) {
		m_context->GetEnemyManager()->ClearAll();
	}
	//UIをクリアする
	if (m_context && m_context->GetUIManager()) {
		m_context->GetUIManager()->Clear();
	}

	//占有者（ユニット参照）をクリアする
	if (m_MapManager) {
		m_MapManager->ClearOccupants();
		m_MapManager->SetScene(nullptr);
	}


	//Contextからプレイヤーへの参照を遮断
	// シーン遷移の瞬間にUIがGetPlayer()を呼び出し、不正なポインタ（野ポインタ）を参照するのを防ぐ
	if (m_context) {
		m_context->SetPlayer(nullptr);
		m_context->SetAlly(nullptr);
	}

	DebugUI::ClearDebugFunction();
}

/**
 * @brief リソースを読み込む
 */
void GameScene::resourceLoader()
{

	{
		// 光源計算なしシェーダー
		std::unique_ptr<CShader> shader = std::make_unique<CShader>();
		shader->Create("shader/unlitTextureVS.hlsl", "shader/unlitTexturePS.hlsl");
		MeshManager::RegisterShader<CShader>("unlightshader", std::move(shader));
	}
 

	// メッシュデータ読み込み（FLOORタイル用）
	{
		std::unique_ptr<CStaticMesh> smesh = std::make_unique<CStaticMesh>();
		smesh->Load("assets/model/obj/floor_1x1x1.obj", "assets/model/obj/");

		std::unique_ptr<CStaticMeshRenderer> srenderer = std::make_unique<CStaticMeshRenderer>();
		srenderer->Init(*smesh);

		if (srenderer->GetMaterial(0)) {
			MATERIAL m = srenderer->GetMaterial(0)->GetData();
			m.Diffuse = Color(1, 1, 1, 1);
			m.TextureEnable = TRUE;
			srenderer->GetMaterial(0)->SetMaterial(m);
		}

		MeshManager::RegisterMesh<CStaticMesh>("floor_mesh", std::move(smesh));
		MeshManager::RegisterMeshRenderer<CStaticMeshRenderer>("floor_mesh", std::move(srenderer));
	}
	//1X1 plane
	{
		std::unique_ptr<CStaticMesh> smesh = std::make_unique<CStaticMesh>();
		smesh->Load("assets/model/obj/range_panel.obj", "assets/model/obj/");

		std::unique_ptr<CStaticMeshRenderer> srenderer = std::make_unique<CStaticMeshRenderer>();
		srenderer->Init(*smesh);

		
		if (srenderer->GetMaterial(0)) {
			MATERIAL m = srenderer->GetMaterial(0)->GetData();
			m.Diffuse = Color(1, 1, 1, 1);
			m.TextureEnable = FALSE; 
			srenderer->GetMaterial(0)->SetMaterial(m);
		}

		MeshManager::RegisterMesh<CStaticMesh>("range_panel_mesh", std::move(smesh));
		MeshManager::RegisterMeshRenderer<CStaticMeshRenderer>("range_panel_mesh", std::move(srenderer));
	}

	// メッシュデータ読み込み（Enemyキャラクター用）
	{
		for (int i = 0; i < 3; ++i)
		{
			std::string fileName = "assets/model/character/enemy_cat_" + std::to_string(i) + ".obj";
			std::string meshName = "enemy_cat_mesh_" + std::to_string(i); // 登録名enemy_cat_mesh_0、enemy_cat_mesh_1…

			std::unique_ptr<CStaticMesh> smesh = std::make_unique<CStaticMesh>();
			// ソースパスassets/model/character/
			smesh->Load(fileName.c_str(), "assets/model/character/");

			std::unique_ptr<CStaticMeshRenderer> srenderer = std::make_unique<CStaticMeshRenderer>();
			srenderer->Init(*smesh);

			MeshManager::RegisterMesh<CStaticMesh>(meshName, std::move(smesh));
			MeshManager::RegisterMeshRenderer<CStaticMeshRenderer>(meshName, std::move(srenderer));

			std::cerr << "   [Resource] Loaded: " << meshName << std::endl;
		}
	}
}

void GameScene::drawGridDebugText()
{
	if (!m_MapManager) return;
// ImGuiの「背景キャンバス」を取得
// これにより、全てのImGuiウィンドウの「裏側」（つまりゲーム画面上）に描画が可能になる
	ImDrawList* drawList = ImGui::GetBackgroundDrawList();

	const auto& allTiles = m_MapManager->GetAllTiles();
	// タイル数を検査する
	ImGui::Text("Tiles: %d", allTiles.size());

	Matrix4x4 view = m_camera->GetViewMatrix();
	Matrix4x4 proj = m_camera->GetProjMatrix();
	float screenWidth = static_cast<float>(Application::GetWidth());
	float screenHeight = static_cast<float>(Application::GetHeight());

	for (const auto& tile : allTiles) 
	{
		Vector3 worldPos = m_MapManager->GetWorldPosition(tile);
		Vector2 screenPos = WorldToScreen(worldPos, view, proj, screenWidth, screenHeight);
	
		if (screenPos.x < 0 || screenPos.x > screenWidth ||
			screenPos.y < 0 || screenPos.y > screenHeight)
		{
			continue;
		}

		char textBuf[32];
		sprintf_s(textBuf, sizeof(textBuf), "[%d,%d]", tile.gridX, tile.gridZ);
		drawList->AddText(ImVec2(screenPos.x, screenPos.y), IM_COL32(255, 255, 0, 255), textBuf);
	}
}

void GameScene::SetGameContext(GameContext* context){
	m_context = context;
	if (m_context) {
		m_MapManager = m_context->GetMapManager();
		m_turnManager = m_context->GetTurnManager();
	}
}

void GameScene::CheckGameStatus(float deltaSeconds)
{
	//まずは失敗条件をチェック
	//もしプレーヤーか味方が死んだら、失敗にします
	bool allyDead = (m_context->GetAlly() && m_context->GetAlly()->GetHP() <= 0);
	bool playerDead = (m_player && m_player->GetHP() <= 0);

	if (allyDead || playerDead)
	{
		//失敗シーンに遷移
		if (!m_isSceneChanging)
		{
			m_isSceneChanging = true;
			SceneManager::GetInstance().SetCurrentScene(
				"GameOverScene",
				std::make_unique<FadeTransition>(1000.0f, FadeTransition::Mode::FadeOutOnly)
			);
		}
		return; //失敗シーンに遷移したら、そのまま戻る
	}

	// 条件A：敵全滅
	bool isEnemyIsAllDead = m_context->GetEnemyManager()->EnemyIsAllDead();

	// 条件B：ターン０になって、脱出成功
	bool isSurvivalWin = (m_remainingTurns <= 0);

	// AもしくはB達成したら、勝利になる
	bool isLogicWin = isEnemyIsAllDead || isSurvivalWin;

	if (isLogicWin)
	{

		//プレーヤーアニメション中になると、待つ
		PlayerState pState = m_player->GetState();
		if (pState == PlayerState::ANIM_MOVE || pState == PlayerState::ANIM_ATTACK)
		{
			return;
		}

		//敵全滅そして全アニメション完了
		if (isEnemyIsAllDead)
		{
			if (!m_context->GetEnemyManager()->EnemyIsEmpty())
			{
				return;
			}
		}

		// 脱出成功なら、敵全アニメション完了待つ
		if (isSurvivalWin)
		{
			if (m_context->GetEnemyManager()->IsAnyEnemyAnimating())
			{
				return;
			}
		}


		// すべてのアニメションが終了したら、クリア処理へ
		m_gameClearTimer += deltaSeconds;

		if (m_gameClearTimer >= GAMECLEAR_WAIT_DURATION)
		{
			if (!m_isSceneChanging)
			{
				m_isSceneChanging = true;
				SceneManager::GetInstance().SetCurrentScene(
					"GameClearScene",
					std::make_unique<FadeTransition>(300.0f, FadeTransition::Mode::FadeInOut)
				);
			}
		}
	}
	else
	{
		//もしクリアしていなければ、タイマーリセット
		m_gameClearTimer = 0.0f;
	}
}

void GameScene::TurnChangeCheck() 
{
	TurnManager* tm = m_context->GetTurnManager();
	EnemyManager* em = m_context->GetEnemyManager();
	//もし敵が全滅していたら、ターン交代処理は行わない
	if (em->EnemyIsAllDead()) {
		return;
	}
	if (tm->IsTurnChangeRequested()) 
	{	//現場の動きは全部終了かのフラグ
		bool isFieldBusy = false;
		//敵は死亡アニメ中かもしくは行動アニメ中か
		if (em->IsAnyEnemyDying() || em->IsAnyEnemyAnimating()) 
		{
			isFieldBusy = true;
		}


		//全部の動き終了後、ターン交代
		if (!isFieldBusy) 
		{
			TurnState current = tm->GetTurnState();

			if(current == TurnState::PlayerPhase) 
			{
				tm->SetState(TurnState::EnemyPhase);
			}
			else if (current == TurnState::EnemyPhase) 
			{
				//敵ターン終了時、全体ターン一つ減る
				m_remainingTurns--;
				if (m_turnCounter) m_turnCounter->SetTurn(m_remainingTurns);

				// 「助けて」の「吹き出し」実行済をレセット
				m_isAllyTalked = false;

				tm->SetState(TurnState::PlayerPhase);
			}
		}
	}
}
