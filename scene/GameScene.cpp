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
#include "../manager/AudioManager.h"
#include "../manager/EffectManager.h"
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

	m_camera = m_context->GetCamera();

	m_camera->ForceSetPolar(Camera::TUTORIAL_RADIUS, Camera::BASE_AZIMUTH, Camera::BASE_ELEVATION);
	// 注視点をマップの中心に向ける（座標を直接指定するか、マップ中心用の定数を作成することも可能）
	m_camera->SetLookAtCenter();

	m_camera->SetState(CameraState::BaseView);

	// ====== 強制的に一度更新し、カメラの正しい物理位置を即座に算出させる ======
	m_camera->Update(1.0f);


	

	// [追加] ゲーム本編BGMの再生
	// loop = true (ループ再生)
	// fadeTime = 2.0f (シーンのフェードイン時間に合わせる)
	AudioManager::GetInstance().PlayBGM("Game", true, 2.0f);

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

	m_MapManager->SetScene(this);
	m_MapManager->Init(m_context);
	std::cerr << "   [GameScene] MapManager OK." << std::endl;
	m_MapManager->LoadLevel("assets/level/level_01.csv", m_context);

	Camera::LoadConfig();
	RecalculateCameraBounds();

	// ====== カメラ境界の動的計算と設定 ======
	RecalculateCameraBounds();
	std::cerr << "   [GameScene] Camera Bounds Auto-Calculated." << std::endl;


	// 背景
	m_background = std::make_unique<Background>();
	m_background->Init();
	std::cerr << "   [GameScene] Background OK" << std::endl;
	//Unit取得
	m_player = m_context->GetPlayer();
	m_ally = m_context->GetAlly();
	EnemyManager* em = m_context->GetEnemyManager();

	m_gameUIManager = m_context->GetUIManager();


	//UI_cutin
	m_turnCutin = std::make_unique<TurnCutin>();
	m_turnCutin->Init();

	m_context->GetTurnManager()->RegisterObserver([this](TurnState state) {
		if (state == TurnState::PlayerPhase) {
			m_turnCutin->PlayCutinAnimation("Player Phase");
			// 【フェーズ1】プレイヤーのターン開始。UIアニメーション再生中に、カメラをプレイヤーへ向けてスムーズに移動させる
			if (m_player) {
				m_camera->SetState(CameraState::Tracking);
				m_camera->SetTargetLookAt(m_player->getSRT().pos);
			}
		}
		else if (state == TurnState::EnemyPhase) {
			m_turnCutin->PlayCutinAnimation("Enemy Phase");
			m_context->GetEnemyManager()->StartEnemyPhase();
			// 【敵ターン】追跡を解除し、全体俯瞰視点（BaseView）へスムーズに戻す
			m_camera->SetState(CameraState::BaseView);
			m_camera->SetTargetToCenter();
			m_camera->SetTargetRadius(Camera::BASE_RADIUS);
		}
		});
	//勝利条件のカウンター
	m_turnCounter = std::make_unique<TurnCounter>();
	m_turnCounter->Init();
	//チュートリアルUI初期化
	m_tutorialUI = std::make_unique<TutorialUI>();
	m_tutorialUI->Init();
	// 吹き出しUI取得
	m_dialogueUI = m_context->GetDialogueUI();
	// ダメージ数字管理取得
	m_damageNumberManager = m_context->GetDamageManager();
	//UI管理取得
	m_gameUIManager = m_context->GetUIManager();

	std::cerr << "   [GameScene] Registering DebugUI..." << std::endl;
	DebugUI::RedistDebugFunction([this]() {
		debugUICamera();
		});

	DebugUI::RedistDebugFunction([this]() {
		drawGridDebugText();
		});



	//std::cerr << "   [GameScene] Starting Turn..." << std::endl;
	//if (m_context->GetTurnManager()) {
	//	m_context->GetTurnManager()->SetState(TurnState::PlayerPhase);
	//}

	//std::cerr << "   [GameScene] Init Finished Success!" << std::endl;

	m_isGameStarted = false;
	m_startDelayTimer = 0.0f;

	m_isSceneChanging = false;

	//敗北ロジック制御変数の初期化
	m_isGameOverProcessing = false;
	m_gameOverTimer = 0.0f;
}


void GameScene::debugUICamera() {
	if (!m_enableDebugCamera) return;

	ImGui::Begin("Player Camera Tuning");

	// 1. 現在がプレイヤーターン（操作フェーズ）かチェック
	bool isPlayerPhase = false;
	if (m_context && m_context->GetTurnManager()) {
		isPlayerPhase = (m_context->GetTurnManager()->GetTurnState() == TurnState::PlayerPhase);
	}

	// プレイヤーターンでない場合は、赤色の警告を表示して操作をロックする
	if (!isPlayerPhase) {
		ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Tuning only available in Player Phase.");
		ImGui::End();
		return;
	}

	// 2. プレイヤーターン中：実際のカメラパラメータを読み取ってバインド
	ImGui::Text("Action Camera Parameters");
	ImGui::Separator();

	bool changed = false;

	// Camera クラスのグローバル inline 変数に直接バインド
	if (ImGui::SliderFloat("Zoom Radius", &Camera::ZOOM_RADIUS, 10.0f, 50.0f, "%.1f")) changed = true;
	if (ImGui::SliderFloat("Azimuth", &Camera::BASE_AZIMUTH, -3.14159f, 3.14159f, "%.4f")) changed = true;
	if (ImGui::SliderFloat("Elevation", &Camera::BASE_ELEVATION, -1.5f, 1.5f, "%.4f")) changed = true;

	// 3. コアメカニズム：パラメータが変更されたら、新しい値をカメラの【目標値 (Target)】に設定
	// これにより、瞬時に切り替わるのではなく Update() 内の Lerp によってスムーズに遷移する
	if (changed && m_camera) {
		m_camera->SetTargetRadius(Camera::ZOOM_RADIUS);
		m_camera->SetTargetAzimuth(Camera::BASE_AZIMUTH);
		m_camera->SetTargetElevation(Camera::BASE_ELEVATION);
	}

	ImGui::Separator();

	// 境界（バウンディングボックス）のパディング設定もこのパネルで一括管理
	if (ImGui::SliderFloat("Bound Padding", &Camera::BOUND_PADDING, -10.0f, 10.0f, "%.1f")) {
		// リアルタイムで境界を再計算し、デバッグ表示を更新
		RecalculateCameraBounds();
	}

	ImGui::Spacing();
	// 4. 設定をローカルの INI ファイルに一括保存
	if (ImGui::Button("Save Config to Local", ImVec2(-1, 30))) {
		Camera::SaveConfig();
	}

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
	// === 最優先でカメラを更新 ===
	// UI Cutin（カットイン）によって後続の更新がブロックされた場合でも、
	// カメラがフェーズ1の移動を継続できるようにここで更新を行う
	if (m_camera) {
		m_camera->Update(deltaSeconds);
	}

	if (!m_isGameStarted)
	{
		m_startDelayTimer += deltaSeconds;

		// FadeInを一秒待ち
		if (m_startDelayTimer >= START_WAIT_TIME)
		{

			// チュートリアルロジックの挿入ポイント
			// チュートリアルUIが存在し、かつ再生が完了していない場合
			if (m_tutorialUI && !m_tutorialUI->IsAllFinished())
			{
				// チュートリアルUIの更新（拡縮アニメーション、入力検知などを処理）
				m_tutorialUI->Update(deltaSeconds);

				// 背景のみを更新し、ゲームのメインロジックには進まない
				if (m_background) m_background->Update(deltatime);
				return;
			}
			m_isGameStarted = true;
			// チュートリアル終了。画面をスムーズに基本の注視距離までズームインさせる
			if (m_camera) {
				m_camera->SetTargetRadius(Camera::BASE_RADIUS);
			}

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

	//ダメージ数字の更新
	if (m_damageNumberManager) {
		m_damageNumberManager->Update(deltatime);
	}

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

	if (m_context->GetEffectManager()) {
		m_context->GetEffectManager()->Update(deltaSeconds);
	}

	AudioManager::GetInstance().Update(deltaSeconds);

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
void GameScene::draw(uint64_t deltatime) {
	// === 1. 背景レイヤー (Background) ===
	Renderer::SetDepthEnable(false);
	if (m_background) m_background->Draw();

	// デプスを再度有効化し、汎用シェーダーを適用
	Renderer::SetDepthEnable(true);

	m_camera->Draw();

	if (m_tileShader != nullptr) { m_tileShader->SetGPU(); }

	// === 2. 画面底面（床）レイヤー (Floor) ===
	for (const auto& obj : m_GameObjectList) {
		MapObject* mapObj = dynamic_cast<MapObject*>(obj.get());
		if (mapObj && mapObj->GetType() == MapModelType::FLOOR) {
			obj->Draw(deltatime);
		}
	}

	// === 3. 床面 UI レイヤー (Floor Hints) ===
	Renderer::SetBlendState(BS_ALPHABLEND);
	for (const auto& obj : m_GameObjectList) {
		obj->DrawFloorUI(deltatime);
	}
	Renderer::SetBlendState(BS_NONE);
	if (m_tileShader) m_tileShader->SetGPU(); // シェーダー状態を復元

	// === 4. 床面特殊オブジェクトレイヤー (Trap) ===
	for (const auto& obj : m_GameObjectList) {
		MapObject* mapObj = dynamic_cast<MapObject*>(obj.get());
		if (mapObj && mapObj->GetType() == MapModelType::TRAP) {
			obj->Draw(deltatime);
		}
	}

	// === 5. 実体エンティティレイヤー ===
	// 5.1 不透明エンティティ (Prop, Wall, Units)
	for (const auto& obj : m_GameObjectList) {
		MapObject* mapObj = dynamic_cast<MapObject*>(obj.get());
		if (mapObj) {
			if (mapObj->GetType() != MapModelType::FLOOR && mapObj->GetType() != MapModelType::TRAP) {
				obj->Draw(deltatime);
			}
		}
		else {
			obj->Draw(deltatime); // Player, Enemy, Ally
		}
	}

	// 5.2 物理パーティクルエフェクト (瓦礫など。デプスによる遮蔽あり)
	Renderer::SetBlendState(BS_ALPHABLEND);
	if (m_context->GetEffectManager()) {
		m_context->GetEffectManager()->DrawRubble();
	}

	// 5.3 半透明エンティティレイヤー (残像など)
	for (const auto& obj : m_GameObjectList) {
		obj->DrawTransparent(deltatime);
	}
	Renderer::SetBlendState(BS_NONE);

	// === 6. 攻撃プレビューヒントレイヤー (Overlay) ===
	Renderer::SetDepthEnable(false);   // 物理的な遮蔽を無視
	Renderer::SetBlendState(BS_ALPHABLEND);
	for (const auto& obj : m_GameObjectList) {
		obj->DrawOverlay(deltatime);   // 浮遊矢印、ヒット警告エフェクトなど
	}

	// === 7. ダメージ・ヒットエフェクトレイヤー ===
	if (m_context->GetEffectManager()) {
		m_context->GetEffectManager()->DrawHitEffects(); // 十字閃光など
	}
	if (m_damageNumberManager) {
		m_damageNumberManager->Draw();
	}

	// === 8. UI (2D) インタラクションレイヤー ===
	// このレイヤーはデプステストを無効に維持
	// 8.1 低層 UI
	if (m_player) m_player->DrawUI();
	if (m_ally && m_ally->GetHP() > 0) m_ally->DrawUI();
	for (const auto& obj : m_GameObjectList) {
		Enemy* enemy = dynamic_cast<Enemy*>(obj.get());
		if (enemy && !enemy->IsDead()) enemy->DrawUI();
	}
	if (m_dialogueUI) m_dialogueUI->Draw();

	// 8.2 高層 UI
	if (m_gameUIManager) m_gameUIManager->Draw();
	if (m_turnCounter) m_turnCounter->Draw();
	if (m_tutorialUI && !m_isGameStarted) m_tutorialUI->Draw();

	// 8.5 最前面 (カットイン演出)
	if (m_turnCutin) m_turnCutin->Draw();

	// === デフォルトの描画状態に戻す ===
	Renderer::SetDepthEnable(true);
	Renderer::SetBlendState(BS_NONE);
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

	if (m_context->GetEffectManager()) {
		m_context->GetEffectManager()->Clear();
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

	//家具のメッシュデータ読み込み（Wallタイル用）
	{
		std::unique_ptr<CStaticMesh> mesh = std::make_unique<CStaticMesh>();

		mesh->Load("assets/model/obj/1x1x1_wall.obj", "assets/model/obj/");

		std::unique_ptr<CStaticMeshRenderer> renderer = std::make_unique<CStaticMeshRenderer>();
		renderer->Init(*mesh);


		MeshManager::RegisterMesh<CStaticMesh>("wall_mesh", std::move(mesh));
		MeshManager::RegisterMeshRenderer<CStaticMeshRenderer>("wall_mesh", std::move(renderer));
	}
	
	//Prop Plane
	{
		std::unique_ptr<CStaticMesh> smesh_prop = std::make_unique<CStaticMesh>();
		
		smesh_prop->Load("assets/model/obj/prop_plane.obj", "assets/model/obj/");


			std::unique_ptr<CStaticMeshRenderer> srender_prop = std::make_unique<CStaticMeshRenderer>();
			srender_prop->Init(*smesh_prop);

			if (auto* mat = srender_prop->GetMaterial(0)) {
				MATERIAL m = mat->GetData();
				m.Diffuse = Color(1, 1, 1, 1);
				m.TextureEnable = FALSE;
				mat->SetMaterial(m);
			}

			MeshManager::RegisterMesh("prop_plane_mesh", std::move(smesh_prop));
			MeshManager::RegisterMeshRenderer("prop_plane_mesh", std::move(srender_prop));
		}
	
	// トラプト用平面メッシュ読み込み
	{
		std::unique_ptr<CStaticMesh> mesh = std::make_unique<CStaticMesh>();

		mesh->Load("assets/model/obj/trap_plane.obj", "assets/model/obj/");

		std::unique_ptr<CStaticMeshRenderer> renderer = std::make_unique<CStaticMeshRenderer>();
		renderer->Init(*mesh);

		if (auto* mat = renderer->GetMaterial(0)) {
			MATERIAL m = mat->GetData();
			m.Diffuse = Color(1, 1, 1, 1);
			m.TextureEnable = FALSE;
			mat->SetMaterial(m);
		}

		MeshManager::RegisterMesh<CStaticMesh>("trap_plane_mesh", std::move(mesh));
		MeshManager::RegisterMeshRenderer<CStaticMeshRenderer>("trap_plane_mesh", std::move(renderer));
	}

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

void GameScene::drawGridDebugText()
{
	if (!m_MapManager) return;
// ImGuiの「背景キャンバス」を取得
// これにより、全てのImGuiウィンドウの「裏側」（つまりゲーム画面上）に描画が可能になる
	ImDrawList* drawList = ImGui::GetForegroundDrawList();

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
		sprintf_s(textBuf, sizeof(textBuf), "[%d,%d]\n(%.1f, %.1f, %.1f)",
			tile.gridX, tile.gridZ,
			worldPos.x, worldPos.y, worldPos.z);
		drawList->AddText(ImVec2(screenPos.x, screenPos.y), IM_COL32(255, 0, 0, 255), textBuf);
	}

	// ====== 【新規】赤色のカメラ境界線 (Camera Bounds) を描画 ======
	Vector3 p1(m_camera->GetBoundMinX(), 0.1f, m_camera->GetBoundMinZ());
	Vector3 p2(m_camera->GetBoundMaxX(), 0.1f, m_camera->GetBoundMinZ());
	Vector3 p3(m_camera->GetBoundMaxX(), 0.1f, m_camera->GetBoundMaxZ());
	Vector3 p4(m_camera->GetBoundMinX(), 0.1f, m_camera->GetBoundMaxZ());

	// ワールド座標からスクリーン座標へ変換
	Vector2 s1 = WorldToScreen(p1, view, proj, screenWidth, screenHeight);
	Vector2 s2 = WorldToScreen(p2, view, proj, screenWidth, screenHeight);
	Vector2 s3 = WorldToScreen(p3, view, proj, screenWidth, screenHeight);
	Vector2 s4 = WorldToScreen(p4, view, proj, screenWidth, screenHeight);

	ImU32 boundCol = IM_COL32(255, 0, 0, 255); // 赤色
	drawList->AddLine(ImVec2(s1.x, s1.y), ImVec2(s2.x, s2.y), boundCol, 2.0f);
	drawList->AddLine(ImVec2(s2.x, s2.y), ImVec2(s3.x, s3.y), boundCol, 2.0f);
	drawList->AddLine(ImVec2(s3.x, s3.y), ImVec2(s4.x, s4.y), boundCol, 2.0f);
	drawList->AddLine(ImVec2(s4.x, s4.y), ImVec2(s1.x, s1.y), boundCol, 2.0f);

	// 境界ラベルの描画
	drawList->AddText(ImVec2(s1.x, s1.y), boundCol, "BOUND MIN");
	drawList->AddText(ImVec2(s3.x, s3.y), boundCol, "BOUND MAX");
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
	// シーン遷移中（フェードアウト中）の場合、重複呼び出し防止のため何もしない
	if (m_isSceneChanging) return;
	//まずは失敗条件をチェック
	//もしプレーヤーか味方が死んだら、失敗にします
	bool allyDead = (m_context->GetAlly() && m_context->GetAlly()->GetHP() <= 0);
	bool playerDead = (m_player && m_player->GetHP() <= 0);
	bool isGameOverCondition = (allyDead || playerDead);

	if (isGameOverCondition || m_isGameOverProcessing)
	{
		// 初めて敗北を検知した場合：ステータスをロック
		if (!m_isGameOverProcessing) {
			m_isGameOverProcessing = true;
			m_gameOverTimer = 0.0f;
			std::cout << "[GameScene] GameOver detected! Waiting for animation..." << std::endl;
		}

		// 経過時間をタイマーに加算
		m_gameOverTimer += deltaSeconds;

		// 待ち時間が経過（2秒）した後、SceneManagerにシーン遷移を通知
		if (m_gameOverTimer >= GAMEOVER_WAIT_DURATION)
		{
			m_isSceneChanging = true;
			SceneManager::GetInstance().SetCurrentScene(
				"GameOverScene",
				std::make_unique<FadeTransition>(1000.0f, FadeTransition::Mode::FadeInOut)//// FadeInOutを使用してスムーズな遷移を確保し、画面のちらつきや急な切り替わりを防止する。
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

void GameScene::RecalculateCameraBounds()
{
	if (!m_MapManager || !m_camera) return;

	const auto& allTiles = m_MapManager->GetAllTiles();
	if (!allTiles.empty()) {
		float minX = 99999.0f, maxX = -99999.0f;
		float minZ = 99999.0f, maxZ = -99999.0f;

		for (const auto& tile : allTiles) {
			Vector3 pos = m_MapManager->GetWorldPosition(tile);
			if (pos.x < minX) minX = pos.x;
			if (pos.x > maxX) maxX = pos.x;
			if (pos.z < minZ) minZ = pos.z;
			if (pos.z > maxZ) maxZ = pos.z;
		}

		m_camera->SetBounds(
			minX - Camera::BOUND_PADDING,
			maxX + Camera::BOUND_PADDING,
			minZ - Camera::BOUND_PADDING,
			maxZ + Camera::BOUND_PADDING
		);
	}
}