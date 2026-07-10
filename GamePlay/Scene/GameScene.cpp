#include "GameScene.h"
#include "GameSceneDebugUI.h"
#include "IntroDirector.h"
#include "GameResultJudge.h"
#include "../../System/Utility/ScreenToWorld.h"
#include "../../System/Utility/WorldToScreen.h"
#include "../../System/CDirectInput.h"
#include "../../System/SphereDrawer.h"
#include "../../System/DebugUI.h"
#include "../../System/CPolar3D.h"
#include "../../System/meshmanager.h"
#include "../../System/scenemanager.h"
#include "../../System//FadeTransition.h"
#include "../../System/BackgroundTransition.h"
#include "../../System/ZFightTunables.h"
#include "../../System/Audio/AudioManager.h"
#include "../../System/ModelRegistry.h"
#include "../../Core/GameContext.h"
#include "../../Core/DebugLog.h"
#include "../../GamePlay/Manager/MapManager.h"
#include "../../GamePlay/Manager/EffectManager.h"
#include "../../GamePlay/Manager/EnemyManager.h"
#include "../../UI/Component/HPBar.h"
#include "../../UI/System/GameUIManager.h"
#include "../../UI/System/DamageNumberManager.h"
#include "../../UI/Component/DialogueUI.h"
#include <stdio.h> // for sprintf_s



namespace {
	// 演出用定数
	const int INITIAL_TURN_COUNT = 5;                  // 初期ターン数（脱出イベントまでのカウントダウン開始値）
	const float ESCAPE_MARKER_BASE_Y = 1.5f;           // 脱出アイコンのY軸ベース座標
}

GameScene::GameScene()
{

}
GameScene::~GameScene() = default;
// ---------------------------------------------------------

// IScene 継承ライフサイクル (Lifecycle Overrides)

// ---------------------------------------------------

// シーンの初期化：処理の全体フローを「目次化」して可読性を高める
void GameScene::Init() {
	DBG_ERROR("=== GameScene::Init Start ===");

	ResetManagers();            // 1. 残留データのクリーンアップ（前回のゾンビコールバック対策）
	InitializeCamera();         // 2. カメラの初期化とフォーカス設定
	LoadGameResources();        // 3. グラフィックリソース・BGMの準備
	InitializeMap();            // 4. ステージ構築とカメラ境界の自動計算
	SetupGameEntities();        // 5. ロジックエンティティ（プレイヤー・敵等）のバインド
	SetupUserInterface();       // 6. UIとターン進行イベントのバインド
	InitializeDebugFeatures();  // 7. 開発・チューニング用ツールのマウント

	m_isGameStarted = false;
	m_startDelayTimer = 0.0f;
	m_resultJudge = std::make_unique<GameResultJudge>(m_context);  // 勝敗判定（毎回生成で状態を全リセット）
}

// シーンの更新処理：表示・ロジック・フロー制御の優先度順に実行
void GameScene::update(uint64_t deltatime)
{
	float deltaSeconds = static_cast<float>(deltatime) / 1000.0f;

	// 1. システム・表現の最優先更新（ロジック停止中も画面をフリーズさせないため）
	UpdateCoreTimers(deltaSeconds);
	UpdateCameraFocus(deltaSeconds);

	// 2. フロー制御インターセプト（チュートリアルや演出中は後続の入力を遮断）
	if (HandlePreGameBlocking(deltatime, deltaSeconds)) return;
	UpdateEnvironmentAndDamageUI(deltatime);
	if (HandleTurnCutinBlocking(deltatime)) return;

	// 3. 演出と入力の更新
	UpdateTurnIntroSequence(deltatime, deltaSeconds);
	HandleCameraRotationInput();

	// ターンUI飛行中などのアニメーション中は、盤面のエンティティ更新を阻止する
	if (IsTurnCounterAnimating()) return;

	// 4. メインロジック・エンティティ更新
	if (m_gameUIManager) m_gameUIManager->Update(deltatime);
	ProcessEscapeEvent();
	// 世界だけ slow-mo（相机/UI は等速）：KillSlow 中 worldScale<1
	float    worldScale = m_camera ? m_camera->GetTimeScale() : 1.0f;
	uint64_t worldDelta = static_cast<uint64_t>(deltatime * worldScale);
	UpdateGameObjects(worldDelta);
	ProcessAllyTacticalDialogue();

	// 5. サブシステムとゲーム進行状態の評価
	UpdatePostEffectsAndAudio(deltatime, deltaSeconds);
	TurnChangeCheck();
	if (m_resultJudge) m_resultJudge->Update(deltaSeconds);
}
// 描画処理：戦術ゲーム特有のレイヤー仕様（Zオーダー）を厳守するパイプライン
void GameScene::draw(uint64_t deltatime) {
	// 1. 最背面：環境背景の描画
	DrawBackgroundLayer();

	if (m_camera) m_camera->Draw();
	if (m_tileShader != nullptr) m_tileShader->SetGPU();

	// 2. レイヤー1：画面底面（床）の描画
	DrawFloorLayer(deltatime);

	// 3. レイヤー2：床面 UI レイヤー (Floor Hints)
	DrawFloorUIHints(deltatime);// 床の上に直接ペイントされるUI。後続のトラップ等に覆い隠されるよう先に描画する

	// 4. レイヤー3：床面特殊オブジェクト (Trap) と 実体エンティティ
	DrawEnvironmentAndEntities(deltatime);	// 床面UIの上にしっかりと乗るように、UIの後に不透明描画を行う

	// 5. レイヤー4：空間パーティクルと半透明オブジェクト
	DrawTransparentWorld(deltatime);

	// 6. レイヤー5：戦術オーバーレイ（最前面の3D空間UI）
	DrawTacticalOverlays(deltatime);

	// 7. エフェクトと2DスクリーンUI
	Renderer::DrawVignette();
	DrawDamageAndHitEffects();
	DrawScreenSpaceUI();
}

// シーン破棄時の安全処理：野ポインタ（Dangling Pointer）によるクラッシュを防止
void GameScene::dispose()
{
	if (m_context && m_damageNumberManager) {
		m_damageNumberManager->ClearAll();
	}
	if (m_context && m_context->GetTurnManager()) {
		m_context->GetTurnManager()->ClearObservers();
	}
	if (m_context && m_context->GetEnemyManager()) {
		m_context->GetEnemyManager()->ClearAll();
	}
	if (m_context && m_context->GetUIManager()) {
		m_context->GetUIManager()->Clear();
	}

	//占有者（ユニット参照）をクリアする
	if (m_MapManager) {
		m_MapManager->ClearOccupants();
		m_MapManager->SetScene(nullptr);
	}


	// シーン遷移の瞬間にUIがGetPlayer()を呼び出し、不正アクセスするのを遮断
	if (m_context) {
		m_context->SetPlayer(nullptr);
		m_context->SetAlly(nullptr);
	}

	if (m_context->GetEffectManager()) {
		m_context->GetEffectManager()->Clear();
	}

	DebugUI::ClearDebugFunction();
}


// ---------------------------------------------------------

// 外部インターフェース・初期設定 (Public Interfaces)

// ---------------------------------------------------------
void GameScene::SetGameContext(GameContext* context) {
	m_context = context;
	if (m_context) {
		m_MapManager = m_context->GetMapManager();
		m_turnManager = m_context->GetTurnManager();
	}
}


// ---------------------------------------------------------
 
// 初期化サブルーチン (Init Sub-routines)

// ---------------------------------------------------------

void GameScene::ResetManagers() {
	// 二重の安全策：前ゲームの残留コールバックやエフェクトを強制消去し、不具合を防止
	if (m_context) {
		if (m_context->GetTurnManager()) {
			DBG_ERROR("   [Init] Clearing Turn Observers...");
			m_context->GetTurnManager()->ClearObservers();
		}
		if (m_context->GetEnemyManager()) m_context->GetEnemyManager()->ClearAll();
		if (m_context->GetUIManager()) m_context->GetUIManager()->Clear();
		if (m_context->GetEffectManager()) m_context->GetEffectManager()->Clear();
		if (m_context->GetDamageManager()) m_context->GetDamageManager()->ClearAll();

		m_context->SetPlayer(nullptr);
		m_context->SetAlly(nullptr);
	}
}

void GameScene::InitializeCamera() {
	Camera::LoadConfig();
	m_camera = m_context->GetCamera();
	m_camera->ForceSetPolar(Camera::TUTORIAL_RADIUS, Camera::BASE_AZIMUTH, Camera::BASE_ELEVATION);
	m_camera->ChangeState(CameraState::BaseView);
	m_camera->ResetCameraDirection();
	m_camera->Update(1.0f); // 初期View/Proj行列を正確に算出するため一度強制更新
}

void GameScene::LoadGameResources() {
	AudioManager::GetInstance().PlayBGM("Game", true, 2.0f);
	resourceLoader();
	m_tileShader = MeshManager::getShader<CShader>("toonshader");
	if (m_tileShader == nullptr) {
		DBG_ERROR("   [FATAL] Shader 'toonshader' is NULL!");
	}
}

void GameScene::resourceLoader() {
	// 1. シェーダー（名前・VS・PS のテーブル駆動）
	struct ShaderDef { const char* name; const char* vs; const char* ps; };
	const ShaderDef shaderDefs[] = {
		{"toonshader",    "shader/ToonVS.hlsl",    "shader/ToonPS.hlsl"},
		{"outlineshader", "shader/OutlineVS.hlsl", "shader/OutlinePS.hlsl"},
		{"blobshader",    "shader/BlobVS.hlsl",    "shader/BlobPS.hlsl"},
	};
	for (const auto& s : shaderDefs) {
		auto shader = std::make_unique<CShader>();
		shader->Create(s.vs, s.ps);
		MeshManager::RegisterShader<CShader>(s.name, std::move(shader));
	}

	// 2. 無加工モデルの一括登録（追加は表に1行足すだけ）
	struct ModelDef { const char* name; const char* path; const char* texDir; };
	const ModelDef models[] = {
		// --- マップ・地形 ---
		{"floor_mesh",          "Assets/model/backgroud/floorFull.obj",     "Assets/model/backgroud/"},
		{"wall_mesh",           "Assets/model/obj/1x1x1_wall.obj",          "Assets/model/obj/"},
		// --- ギミック・プロップ ---
		{"trap_mesh",           "Assets/model/obj/trap.obj",                "Assets/model/obj/"},
		{"prop_plane_mesh",     "Assets/model/obj/prop_plane.obj",          "Assets/model/obj/"},
		{"sofa_yoko_mesh",      "Assets/model/obj/loungeSofa.obj",          "Assets/model/obj/"},
		{"cattower_mesh",       "Assets/model/obj/coatRackStanding.obj",    "Assets/model/obj/"},
		{"bookshelf_mesh",      "Assets/model/obj/bookcaseClosedDoors.obj", "Assets/model/obj/"},
		{"table_mesh",          "Assets/model/obj/tableCloth.obj",          "Assets/model/obj/"},
		// --- 矢印ナビゲーション ---
		{"arrow_straight_mesh", "Assets/model/obj/arrow_straight.obj",      "Assets/model/obj/"},
		{"arrow_corner_mesh",   "Assets/model/obj/arrow_corner.obj",        "Assets/model/obj/"},
		{"arrow_attack_mesh",   "Assets/model/obj/arrow_attack.obj",        "Assets/model/obj/"},
		{"arrow_push_mesh",     "Assets/model/obj/arrow_push.obj",          "Assets/model/obj/"},
	};
	for (const auto& m : models) {
		ModelRegistry::RegisterModel(m.name, m.path, m.texDir);
	}

	// 3. マテリアル調整が要る例外モデル（差分をコールバックで注入）
	// 白色・テクスチャ無効（頂点カラー/単色でタイル染めする戦術UI用）
	auto whiteNoTex = [](CStaticMeshRenderer& r) {
		if (auto* mat = r.GetMaterial(0)) {
			MATERIAL m = mat->GetData();
			m.Diffuse = Color(1, 1, 1, 1);
			m.TextureEnable = FALSE;
			mat->SetMaterial(m);
		}
		};
	ModelRegistry::RegisterModel("trap_plane_mesh", "Assets/model/obj/trap_plane.obj", "Assets/model/obj/", whiteNoTex);
	ModelRegistry::RegisterModel("range_panel_mesh", "Assets/model/obj/range_panel.obj", "Assets/model/obj/", whiteNoTex);

	// 脱出マス：空色・半透明
	ModelRegistry::RegisterModel("escape_cube_mesh", "Assets/model/obj/floor_1x1x1.obj", "Assets/model/obj/",
		[](CStaticMeshRenderer& r) {
			if (auto* mat = r.GetMaterial(0)) {
				MATERIAL m = mat->GetData();
				m.Diffuse = Color(135.0f / 255.0f, 206.0f / 255.0f, 250.0f / 255.0f, 0.6f);
				m.TextureEnable = FALSE;
				mat->SetMaterial(m);
			}
		});

	// 4. 2Dスプライト・UI画像
	m_escapeMarkerSprite = std::make_unique<CSprite>(128, 128, "Assets/texture/ui/escape_marker.png");
	m_winTextSprite = std::make_unique<CSprite>(308, 205, "Assets/texture/ui/win_text.png");
}

void GameScene::InitializeMap() {
	m_MapManager->SetScene(this);
	m_MapManager->Init(m_context);
	DBG_ERROR("   [GameScene] MapManager OK.");
	m_MapManager->LoadLevel("Assets/level/level_01.csv", m_context);

	// マップサイズに基づき、カメラの表示崩れを防ぐ境界範囲を自動計算
	RecalculateCameraBounds();
	DBG_ERROR("   [GameScene] Camera Bounds Auto-Calculated.");
	m_background = std::make_unique<Background>();
	m_background->Init();
	DBG_ERROR("   [GameScene] Background OK");
}

void GameScene::SetupGameEntities() {
	m_player = m_context->GetPlayer();
	m_ally = m_context->GetAlly();
	EnemyManager* em = m_context->GetEnemyManager();
}

void GameScene::SetupUserInterface() {
	m_turnCutin = std::make_unique<TurnCutin>();
	m_turnCutin->Init();

	// ターン進行とカメラ演出のデカップリング（分離設計）
	m_context->GetTurnManager()->RegisterObserver([this](TurnState state) {
		if (state == TurnState::PlayerPhase) {
			if (m_turnCounter) m_turnCounter->Hide();
			m_turnCutin->PlayCutinAnimation("Player Phase");

			m_needsTurnCounterAnim = true;

			if (m_player) {
				// フェーズに応じたプレイヤー誘導：
				// ① 脱出フェーズ中は目的地である味方を注視
				// ② ゲーム開始直後（第1ターン）は守るべき対象（味方）を注視し、プレイヤーに目標を認識させる
				// ③ それ以外の通常ターンは操作対象であるプレイヤー自身を注視
				if (m_isEscapeActive && m_ally && !m_ally->IsEscapeDone()) {
					m_camera->ChangeState(CameraState::TargetFocus, m_ally->getSRT().pos);
				}
				else if (m_remainingTurns != INITIAL_TURN_COUNT) {
					m_camera->ChangeState(CameraState::Tracking, m_player->getSRT().pos);
				}
			}
		}
		else if (state == TurnState::EnemyPhase) {
			m_turnCutin->PlayCutinAnimation("Enemy Phase");
			m_context->GetEnemyManager()->StartEnemyPhase();
			// 敵ターン中は全体状況を把握させるため俯瞰視点へ戻す
			m_camera->ChangeState(CameraState::BaseView);
		}
		});

	m_turnCounter = std::make_unique<TurnCounter>();
	m_turnCounter->Init();
	m_tutorialUI = std::make_unique<TutorialUI>();
	m_tutorialUI->Init();

	m_dialogueUI = m_context->GetDialogueUI();
	m_damageNumberManager = m_context->GetDamageManager();
	m_gameUIManager = m_context->GetUIManager();
	// 導入演出の進行管理（TurnCounter生成後に初期化）
	m_introDirector = std::make_unique<IntroDirector>(m_context, m_turnCounter.get());
}

void GameScene::InitializeDebugFeatures() {
	DBG_ERROR("   [GameScene] Registering DebugUI...");

	m_debugUI = std::make_unique<GameSceneDebugUI>(*this);
	DebugUI::RedistDebugFunction([this]() { m_debugUI->DrawCameraTuningWindow(); });
	DebugUI::RedistDebugFunction([this]() { m_debugUI->DrawGridDebugText(); });
}


// ---------------------------------------------------------

// 更新サブルーチン：システム・表現 (System & Presentation)

// ---------------------------------------------------------

void GameScene::UpdateCoreTimers(float deltaSeconds)
{
	m_uiAnimTimer += deltaSeconds;
}

void GameScene::UpdateCameraFocus(float deltaSeconds)
{
	// UIカットイン等によって後続の更新がブロックされた場合でも、
	
	// 画面が静止してフリーズしたような印象を与えないよう、カメラ移動のみ最優先で継続させる
	if (m_camera) {
		m_camera->Update(deltaSeconds);
	}
}

void GameScene::UpdateTurnIntroSequence(uint64_t deltatime, float deltaSeconds) {
	if (m_needsTurnCounterAnim) {
		if (m_turnCounter) m_turnCounter->StartAnimation();
		m_needsTurnCounterAnim = false;

		// 開幕の演出制御：第1ターン開始時、戦局全体を見せるカメラ演出を挿入
		
		if (m_remainingTurns == INITIAL_TURN_COUNT && m_introDirector && m_introDirector->IsIdle()) {
			m_introDirector->Start();
		}
	}

	if (m_turnCounter) m_turnCounter->Update(deltatime);
	if (m_introDirector) m_introDirector->Update(deltaSeconds, m_isAllyTalked);
}


// ---------------------------------------------------------
 
// 更新サブルーチン：フロー制御インターセプト (Flow Control)

// ---------------------------------------------------------
bool GameScene::HandlePreGameBlocking(uint64_t deltatime, float deltaSeconds) {
	if (m_isGameStarted) return false;

	m_startDelayTimer += deltaSeconds;

	// 画面遷移直後の違和感を防ぐため、一定時間待機してから進行を開始
	if (m_startDelayTimer >= START_WAIT_TIME) {
		if (m_tutorialUI && !m_tutorialUI->IsAllFinished()) {
			m_tutorialUI->Update(deltaSeconds);
			if (m_background) m_background->Update(deltatime);
			return true;
		}

		m_isGameStarted = true;
		if (m_camera) m_camera->SetTargetRadius(Camera::BASE_RADIUS);

		if (m_context && m_context->GetTurnManager()) {
			DBG_ERROR("[GameScene] FadeIn Complete. Start Player Phase!");
			m_context->GetTurnManager()->SetState(TurnState::PlayerPhase);
		}
	}

	if (m_background) m_background->Update(deltatime);
	return true;
}

bool GameScene::HandleTurnCutinBlocking(uint64_t deltatime)
{
	// ターン切り替え時の重要な演出：他の動きをすべて止め、プレイヤーの視線をカットインに集中させる
	if (m_turnCutin && m_turnCutin->IsAnimating()) {
		m_turnCutin->Update(deltatime);
		return true;
	}
	return false;
}

bool GameScene::IsTurnCounterAnimating() const
{
	// ターンカウンター飛行演出中：視覚的な情報過多を防ぐため、盤面の更新を一時停止
	return (m_turnCounter && m_turnCounter->IsAnimating());
}


// ---------------------------------------------------------
 
// 更新サブルーチン：メインロジック (Main Logic & Entities)

// ---------------------------------------------------------

void GameScene::UpdateEnvironmentAndDamageUI(uint64_t deltatime)
{
	if (m_background) m_background->Update(deltatime);
	if (m_damageNumberManager) m_damageNumberManager->Update(deltatime);
}

void GameScene::HandleCameraRotationInput()
{
	// リザルト演出中やシーン遷移中は、カメラの不自然な回転を防ぐため入力を無視する
	bool canRotate = (m_isGameStarted && m_resultJudge &&
		!m_resultJudge->IsGameOverProcessing() && !m_resultJudge->IsSceneChanging());

	if (m_gameUIManager) {
		m_gameUIManager->SetCameraRotateVisible(canRotate);
	}

	if (canRotate) {
		if (CDirectInput::GetInstance().CheckKeyBufferTrigger(DIK_Q)) {
			if (m_camera) m_camera->RotateCameraReverse();
		}
		if (CDirectInput::GetInstance().CheckKeyBufferTrigger(DIK_E)) {
			if (m_camera) m_camera->RotateCameraForward();
		}
	}
}

void GameScene::ProcessEscapeEvent()
{
	if (!m_isEscapeActive || !m_player || !m_ally || !m_ally->IsEscapeDone()) return;

	if (m_player->GetState() == PlayerState::MENU_MAIN &&
		m_player->GetUnitGridX() == m_escapeGridX &&
		m_player->GetUnitGridZ() == m_escapeGridZ) {

		// 脱出成功の強調：カメラを強制的にプレイヤーへズームインさせ達成感を高める
		if (m_camera) {
			m_camera->ChangeState(CameraState::TargetFocus, m_player->getSRT().pos);
		}

		m_player->StartCelebration();
	}
}

void GameScene::UpdateGameObjects(uint64_t deltatime)
{
	for (const auto& obj : m_GameObjectList) {
		obj->Update(deltatime);
	}

	if (m_context && m_context->GetEnemyManager()) {
		m_context->GetEnemyManager()->Update(deltatime);
	}
}

void GameScene::ProcessAllyTacticalDialogue()
{
	if (!m_context || !m_context->GetTurnManager()) return;

	if (m_context->GetTurnManager()->GetTurnState() == TurnState::PlayerPhase) {
		if (m_turnCutin && !m_turnCutin->IsAnimating()) {
			if (!m_isAllyTalked) {
				// 戦術の誘導：プレイヤーターンの開始時、生存している味方から行動のヒントを提示
				if (m_ally && m_ally->GetHP() > 0) {
					Vector3 allyPos = m_ally->getSRT().pos;
					if (m_dialogueUI) {
						m_dialogueUI->ShowDialogue(allyPos, DialogueType::Help);
					}
				}
				m_isAllyTalked = true;
			}
		}
	}
}

void GameScene::UpdatePostEffectsAndAudio(uint64_t deltatime, float deltaSeconds)
{
	if (m_context && m_context->GetDialogueUI()) {
		m_context->GetDialogueUI()->Update(deltatime);
	}
	if (m_context && m_context->GetEffectManager()) {
		m_context->GetEffectManager()->Update(deltaSeconds);
	}
	AudioManager::GetInstance().Update(deltaSeconds);
}

// ---------------------------------------------------------
 
// ターン進行とイベント制御 (Turn Flow & Events)
 
// ---------------------------------------------------------
void GameScene::TurnChangeCheck()
{
	TurnManager* tm = m_context->GetTurnManager();
	EnemyManager* em = m_context->GetEnemyManager();
	// ゲームオーバー確定後はターンを進めない（切替時に Player/Enemy Phase 演出が誤再生されるのを防ぐ）
	if (m_resultJudge && (m_resultJudge->IsGameOverProcessing() || m_resultJudge->IsGameOverCondition())) return;

	// 1. ターン交代の必要がない、または敵が全滅している場合は処理しない
	if (!tm->IsTurnChangeRequested() || em->AreAllEnemiesDead()) {
		return;
	}

	// 2. フィールド上のブロック判定（敵の死亡・行動アニメーション中か）
	if (em->IsAnyEnemyDying() || em->IsAnyEnemyAnimating()|| (m_camera && m_camera->IsCinematic())){
		return; // アニメーション終了までターン交代を待機
	}

	// 3. 状態遷移（State Transition）の実行
	TurnState current = tm->GetTurnState();

	if (current == TurnState::PlayerPhase) {
		tm->SetState(TurnState::EnemyPhase);
	}
	else if (current == TurnState::EnemyPhase) {
		// 敵ターン終了時の各種ゲーム内イベント（脱出判定など）を処理
		ProcessEndOfEnemyPhase();

		// プレイヤーのターンへ移行
		tm->SetState(TurnState::PlayerPhase);
	}
}

void GameScene::ProcessEndOfEnemyPhase()
{
	m_remainingTurns--;
	if (m_remainingTurns < 0) {
		m_remainingTurns = 0; // 負の値にならないようクランプ（防御的プログラミング）
	}

	// UIの更新
	if (m_turnCounter) m_turnCounter->SetTurn(m_remainingTurns);

	// --- ターンの進行に伴う特殊イベントの評価 ---
	CheckAndTriggerEscapeEvent();

	// 味方が脱出後、味方の会話フラグをリセット
	m_isAllyTalked = false;

}

void GameScene::CheckAndTriggerEscapeEvent()
{
	// 規定ターン（0）に到達し、かつまだ脱出フェーズが起動していない場合のみ実行
	if (m_remainingTurns <= 0 && !m_isEscapeActive) {

		m_isEscapeActive = true;

		if (m_ally) {
			m_escapeGridX = m_ally->GetUnitGridX();
			m_escapeGridZ = m_ally->GetUnitGridZ();

			m_ally->TriggerEscape();

			if (m_dialogueUI) {
				m_dialogueUI->ShowDialogue(m_ally->getSRT().pos, DialogueType::Escape);
			}
			DBG_ERROR("[GameEvent] Survival Phase Ended. Escape Triggered at ("
				<< m_escapeGridX << ", " << m_escapeGridZ << ").");
		}
	}
}


// ---------------------------------------------------------
 
// レンダリングパイプライン (Rendering Sub-routines)
 
// ---------------------------------------------------------

void GameScene::DrawBackgroundLayer() {
	// 背景は常に最も遠くに存在するため、深度テストを無効にして画面全体を塗りつぶす
	Renderer::SetDepthEnable(false);
	if (m_background) m_background->Draw();
	Renderer::SetDepthEnable(true); 
}

void GameScene::DrawFloorLayer(uint64_t deltatime) {
	for (const auto& obj : m_GameObjectList) {
		MapObject* mapObj = dynamic_cast<MapObject*>(obj.get());
		if (mapObj && mapObj->GetType() == MapModelType::FLOOR) {
			obj->Draw(deltatime);
		}
	}
}

void GameScene::DrawFloorUIHints(uint64_t deltatime) {
	Renderer::SetBlendState(BS_ALPHABLEND);
	for (const auto& obj : m_GameObjectList) {
		obj->DrawFloorUI(deltatime);
	}
	Renderer::SetBlendState(BS_NONE);
	if (m_tileShader) m_tileShader->SetGPU(); 
}

void GameScene::DrawEnvironmentAndEntities(uint64_t deltatime) {
	// === 1. 床面特殊オブジェクトレイヤー (Trap) ===
	
	// 先に描画した床面UI（半透明）の上に確実に乗せるため、ここで描画する
	for (const auto& obj : m_GameObjectList) {
		MapObject* mapObj = dynamic_cast<MapObject*>(obj.get());
		if (mapObj && mapObj->GetType() == MapModelType::TRAP) {
			obj->Draw(deltatime);
		}
	}

	// === 2. 実体エンティティレイヤー (Props, Walls, Characters) ===
	for (const auto& obj : m_GameObjectList) {
		MapObject* mapObj = dynamic_cast<MapObject*>(obj.get());
		if (mapObj) {
			// 床とトラップ以外のマップオブジェクトを描画
			if (mapObj->GetType() != MapModelType::FLOOR && mapObj->GetType() != MapModelType::TRAP) {
				obj->Draw(deltatime);
			}
		}
		else {
			// キャラクター（Player, Enemy, Ally）を描画
			obj->Draw(deltatime);
		}
	}
}

void GameScene::DrawTransparentWorld(uint64_t deltatime) {
	Renderer::SetBlendState(BS_ALPHABLEND);

	// 物理パーティクル（瓦礫など。深度テストによる遮蔽を有効にする）
	if (m_context && m_context->GetEffectManager()) {
		m_context->GetEffectManager()->DrawRubble();
	}

	// 半透明エンティティ（残像など）
	for (const auto& obj : m_GameObjectList) {
		obj->DrawTransparent(deltatime);
	}

	// 仲間の脱出（採掘・フェードアウト）が完了した後のみ、空色のマスを描画
	bool shouldDrawEscape = (m_isEscapeActive && m_ally && m_ally->IsEscapeDone()) || m_shouldShowDebugEscape; 
	if (shouldDrawEscape) {
		DrawEscapeCube();
	}

	Renderer::SetBlendState(BS_NONE);
}

void GameScene::DrawTacticalOverlays(uint64_t deltatime) {
	// 戦術オーバーレイ：壁や他のキャラクターに隠れて見えなくなるのを防ぐため、深度計算をスキップする
	Renderer::SetDepthEnable(false);
	Renderer::SetBlendState(BS_ALPHABLEND);

	for (const auto& obj : m_GameObjectList) {
		obj->DrawOverlay(deltatime);   // 浮遊矢印、ヒット警告エフェクトなど
	}

	Renderer::SetBlendState(BS_NONE);
	Renderer::SetDepthEnable(true);
}

void GameScene::DrawDamageAndHitEffects() {
	if (m_context && m_context->GetEffectManager()) {
		m_context->GetEffectManager()->DrawHitEffects(); // 攻撃エフェクト（スパークなど）
	}
	if (m_damageNumberManager) {
		m_damageNumberManager->Draw();
	}
}

void GameScene::DrawScreenSpaceUI() {
	// UI描画モード開始：深度テストを完全にオフにし、UI専用のサンプラーを適用
	Renderer::SetUISamplerMode(true);
	Renderer::SetDepthEnable(false);

	// --- 1. キャラクター追従UI (低層) ---
	if (m_player) m_player->DrawUI();
	if (m_ally && m_ally->GetHP() > 0) m_ally->DrawUI();
	for (const auto& obj : m_GameObjectList) {
		Enemy* enemy = dynamic_cast<Enemy*>(obj.get());
		if (enemy && !enemy->IsDead()) enemy->DrawUI();
	}
	if (m_dialogueUI) m_dialogueUI->Draw();

	// --- 2. 特定状況下のポップアップ ---
	bool shouldDrawEscape = (m_isEscapeActive && m_ally && m_ally->IsEscapeDone()) || m_shouldShowDebugEscape;
	if (shouldDrawEscape && m_player && m_player->GetState() != PlayerState::ANIM_CELEBRATE) {
		DrawEscapeMarker();
	}
	if (m_player && m_player->GetState() == PlayerState::ANIM_CELEBRATE) {
		DrawWinText();
	}

	// --- 3. 画面固定のシステムUI (高層) ---
	if (m_gameUIManager && m_showActionUI) m_gameUIManager->Draw();
	if (m_turnCounter) m_turnCounter->Draw();
	if (m_tutorialUI && !m_isGameStarted) m_tutorialUI->Draw();

	// --- 4. 最前面 (カットイン演出) ---
	if (m_turnCutin) m_turnCutin->Draw();

	// UI描画モード終了：デフォルトの3D描画状態へ安全に復帰
	Renderer::SetDepthEnable(true);
	Renderer::SetUISamplerMode(false);
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

void GameScene::DrawEscapeCube() {
	CStaticMeshRenderer* escapeCube = MeshManager::getRenderer<CStaticMeshRenderer>("escape_cube_mesh");
	if (escapeCube) {
		Vector3 pos = m_context->GetMapManager()->GetWorldPosition(m_escapeGridX, m_escapeGridZ);
		pos.y += 1.05f;
		Matrix4x4 world = Matrix4x4::CreateTranslation(pos);
		Renderer::SetWorldMatrix(&world);
		escapeCube->Draw();
	}
}

void GameScene::DrawEscapeMarker() {
	if (!m_escapeMarkerSprite) return;
	Vector3 worldPos = m_context->GetMapManager()->GetWorldPosition(m_escapeGridX, m_escapeGridZ);

	// === 正弦波を利用して上下の浮遊感を計算 ===
	float floatSpeed = 3.0f;       // 浮遊スピード（値が大きいほど速く上下する）
	float floatAmplitude = 0.15f;  // 浮遊の振幅（値が大きいほど上下の範囲が広がる）
	float bobbingOffset = sinf(m_uiAnimTimer * floatSpeed) * floatAmplitude;

	worldPos.y += (ESCAPE_MARKER_BASE_Y + bobbingOffset);

	Matrix4x4 view = m_camera->GetViewMatrix();
	Matrix4x4 proj = m_camera->GetProjMatrix();
	Vector2 screenPos = WorldToScreen(worldPos, view, proj, Application::GetWidth(), Application::GetHeight());

	// 1. マテリアル設定
	MATERIAL mtrl;
	mtrl.Diffuse = Color(1.0f, 1.0f, 1.0f, 1.0f);
	mtrl.TextureEnable = TRUE;
	m_escapeMarkerSprite->ModifyMtrl(mtrl);

	// 2. レンダリングステート
	Renderer::SetDepthEnable(false);
	Renderer::SetBlendState(BS_ALPHABLEND);

	// 3. 描画
	m_escapeMarkerSprite->Draw(Vector3(1, 1, 1), Vector3(0, 0, 0), Vector3(screenPos.x, screenPos.y, 0));

	// 4. ステートの復元
	Renderer::SetBlendState(BS_NONE);
	Renderer::SetDepthEnable(true);
}

void GameScene::DrawWinText() {
	if (!m_winTextSprite || !m_player || !m_context || !m_context->GetMapManager()) return;

	int pX = m_player->GetUnitGridX();
	int pZ = m_player->GetUnitGridZ();
	Vector3 basePos = m_context->GetMapManager()->GetWorldPosition(pX, pZ);
	basePos.x += 0.0f;
	basePos.y += 1.7f;

	Matrix4x4 view = m_camera->GetViewMatrix();
	Matrix4x4 proj = m_camera->GetProjMatrix();
	Vector2 screenPos = WorldToScreen(basePos, view, proj, Application::GetWidth(), Application::GetHeight());

	// 1. マテリアル設定：テクスチャを有効化
	MATERIAL mtrl;
	mtrl.Diffuse = Color(1.0f, 1.0f, 1.0f, 1.0f);
	mtrl.TextureEnable = TRUE;
	m_winTextSprite->ModifyMtrl(mtrl);

	// 2. レンダリングステート：深度テストを無効化（「WIN」を最前面に）、アルファブレンドを有効化
	Renderer::SetUISamplerMode(true);
	Renderer::SetDepthEnable(false);
	Renderer::SetBlendState(BS_ALPHABLEND);

	// 3. 描画（「WIN」画像をスクリーンの中心付近にオフセットして表示）
	m_winTextSprite->Draw(Vector3(1, 1, 1), Vector3(0, 0, 0), Vector3(screenPos.x, screenPos.y, 0));

	// 4. ステートの復元
	Renderer::SetBlendState(BS_NONE);
	Renderer::SetDepthEnable(true);
	Renderer::SetUISamplerMode(false);
}





