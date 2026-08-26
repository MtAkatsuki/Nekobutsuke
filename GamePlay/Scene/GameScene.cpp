#include "GameScene.h"
#include "GameSceneDebugUI.h"
#include "IntroDirector.h"
#include "GameResultJudge.h"
#include "Background.h"
#include "../../Actor/Character/Enemy.h"
#include "../../Actor/Character/Player.h"
#include "../../Actor/Character/Ally.h"
#include "../../System/Utility/WorldToScreen.h"
#include "../../System/CDirectInput.h"
#include "../../System/DebugUI.h"
#include "../../System/CPolar3D.h"
#include "../../System/MeshManager.h"
#include "../../System/SceneManager.h"
#include "../../System//FadeTransition.h"
#include "../../System/BackgroundTransition.h"
#include "../../System/ZFightTunables.h"
#include "../../System/Audio/AudioManager.h"
#include "../../System/ModelRegistry.h"
#include "../../System/CShader.h"
#include "../../System/CSprite.h"
#include "../../Core/GameContext.h"
#include "../../Core/DebugLog.h"
#include "../../GamePlay/Manager/MapManager.h"
#include "../../GamePlay/Manager/EffectManager.h"
#include "../../GamePlay/Manager/EnemyManager.h"
#include "../../UI/Component/HPBar.h"
#include "../../UI/System/GameUIManager.h"
#include "../../UI/System/DamageNumberManager.h"
#include "../../UI/Component/DialogueUI.h"
#include "../../UI/Component/TurnCutin.h"
#include "../../UI/Component/TurnCounter.h"
#include "../../UI/Component/TutorialUI.h"
#include "../../System/FxTunables.h"
#include <stdio.h> // for sprintf_s
#include <cfloat>  // for FLT_MAX
#include <cmath>



namespace {
	// 演出用定数（INITIAL_TURN_COUNT は GameScene.h のクラス定数へ移動）
	const float ESCAPE_MARKER_BASE_Y = 1.5f;           // 脱出アイコンのY軸ベース座標
	const float ESCAPE_MARKER_FLOAT_SPEED = 3.0f;      // 脱出アイコンの浮遊スピード（値が大きいほど速く上下する）
	const float ESCAPE_MARKER_FLOAT_AMPLITUDE = 0.15f; // 脱出アイコンの浮遊振幅（値が大きいほど上下の範囲が広がる）
	const float ESCAPE_CUBE_Y_OFFSET = 1.05f;          // 脱出マスキューブの床からの浮かせ量
	const Color ESCAPE_CUBE_COLOR = Color(135.0f / 255.0f, 206.0f / 255.0f, 250.0f / 255.0f, 0.6f); // 空色・半透明
	const float WIN_TEXT_Y_OFFSET = 1.7f;              // WIN表示のプレイヤー頭上オフセット
	const float BGM_FADE_TIME = 2.0f;                  // ゲームBGMのフェードイン時間（秒）
	const float ALLY_HELP_DURATION = 3.0f;        // 通常ターンの救援吹き出し表示時間（秒）
	const float INTRO_ALLY_HELP_DURATION = 4.5f;  // 導入演出中の吹き出し表示時間（＝味方特写の滞在時間）
	const float UNIT_OCCLUDE_RADIUS = 0.6f;   // ユニットの遮蔽判定半径（カメラと注視対象を結ぶ線の周囲）

	// スプライトのテクスチャ実寸（px）
	const float ESCAPE_MARKER_TEX_SIZE = 128.0f;
	const float WIN_TEXT_TEX_W = 308.0f;
	const float WIN_TEXT_TEX_H = 205.0f;
}

// ターンカウンターの数字テクスチャ枚数を超える初期ターン数は表示できない
static_assert(GameScene::INITIAL_TURN_COUNT <= TurnCounter::MAX_TURN_SPRITES,
	"INITIAL_TURN_COUNT exceeds available turn number sprites");

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
void GameScene::Update(float deltaSeconds)
{

	// 1. システム・表現の最優先更新（ロジック停止中も画面をフリーズさせないため）
	UpdateCoreTimers(deltaSeconds);
	UpdateCameraFocus(deltaSeconds);
	UpdateOcclusionFade();//dither fade out

	// 2. フロー制御インターセプト（チュートリアルや演出中は後続の入力を遮断）
	if (HandlePreGameBlocking(deltaSeconds)) return;
	UpdateEnvironmentAndDamageUI(deltaSeconds);
	MaybePlayPendingTurnCutin();
	if (HandleTurnCutinBlocking(deltaSeconds)) return;

	// 3. 演出と入力の更新
	UpdateTurnIntroSequence(deltaSeconds);
	HandleCameraRotationInput();

	// ターンUI飛行中などのアニメーション中は、盤面のエンティティ更新を阻止する
	if (IsTurnCounterAnimating()) return;

	// 4. メインロジック・エンティティ更新
	if (m_gameUIManager) m_gameUIManager->Update(deltaSeconds);
	ProcessEscapeEvent();
	// 世界だけ slow-mo（相机/UI は等速）：KillSlow 中 worldScale<1
	float worldScale = m_camera ? m_camera->GetTimeScale() : 1.0f;
	float worldDelta = deltaSeconds * worldScale;
	UpdateGameObjects(worldDelta);
	RemoveDeadObjects();
	ProcessAllyTacticalDialogue();

	// 5. サブシステムとゲーム進行状態の評価
	UpdatePostEffectsAndAudio(deltaSeconds);
	TurnChangeCheck();
	if (m_resultJudge) m_resultJudge->Update(deltaSeconds);
}
// 描画処理：戦術ゲーム特有のレイヤー仕様（Zオーダー）を厳守するパイプライン
void GameScene::Draw(float deltaSeconds) {
	// 1. 最背面：環境背景の描画
	DrawBackgroundLayer();

	if (m_camera) m_camera->Draw();
	if (m_tileShader != nullptr) m_tileShader->SetGPU();

	// 2. レイヤー1：画面底面（床）の描画
	DrawFloorLayer(deltaSeconds);

	// 3. レイヤー2：床面 UI レイヤー (Floor Hints)
	DrawFloorUIHints(deltaSeconds);// 床の上に直接ペイントされるUI。後続のトラップ等に覆い隠されるよう先に描画する

	// 4. レイヤー3：床面特殊オブジェクト (Trap) と 実体エンティティ
	DrawEnvironmentAndEntities(deltaSeconds);	// 床面UIの上にしっかりと乗るように、UIの後に不透明描画を行う

	// 5. レイヤー4：空間パーティクルと半透明オブジェクト
	DrawTransparentWorld(deltaSeconds);

	// 6. レイヤー5：戦術オーバーレイ（最前面の3D空間UI）
	DrawTacticalOverlays(deltaSeconds);

	// 7. エフェクトと2DスクリーンUI
	Renderer::DrawVignette();
	DrawDamageAndHitEffects();
	DrawScreenSpaceUI();
}

void GameScene::RemoveDeadObjects()
{
	// 削除前に生ポインタの別名（エイリアス）を無効化する
	for (const auto& obj : m_gameObjectList) {
		if (!obj->IsDead()) continue;
		if (m_context) {
			if (obj.get() == static_cast<GameObject*>(m_context->GetPlayer())) {
				m_context->SetPlayer(nullptr);
				m_player = nullptr;
			}
			// KillCam の追従対象が破棄される場合は参照を無効化
			if (obj.get() == static_cast<GameObject*>(m_killCamTrackTarget)) {
				m_killCamTrackTarget = nullptr;
			}
			if (obj.get() == static_cast<GameObject*>(m_context->GetAlly())) {
				m_context->SetAlly(nullptr);
				m_ally = nullptr;
			}

		}
	}

	// unique_ptr の破棄により ~Unit が TurnManager の購読を自動解除する
	std::erase_if(m_gameObjectList,
		[](const std::unique_ptr<GameObject>& o) { return o->IsDead(); });
}

// シーン破棄時の安全処理：野ポインタ（Dangling Pointer）によるクラッシュを防止
void GameScene::Dispose()
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
	if (m_mapManager) {
		m_mapManager->ClearOccupants();
		m_mapManager->SetScene(nullptr);
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
		m_mapManager = m_context->GetMapManager();
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
	Fx::LoadConfig();   // FX パラメータの INI 上書き（無ければヘッダ既定値）
	m_camera = m_context->GetCamera();
	m_camera->ForceSetPolar(Camera::TUTORIAL_RADIUS, Camera::BASE_AZIMUTH, Camera::BASE_ELEVATION);
	m_camera->ChangeState(CameraState::BaseView);
	m_camera->ResetCameraDirection();
	m_camera->EnterStrategyView();
	m_camera->Update(1.0f); // 初期View/Proj行列を正確に算出するため一度強制更新
}

void GameScene::LoadGameResources() {
	AudioManager::GetInstance().PlayBGM("Game", true, BGM_FADE_TIME);
	LoadRenderResources();
	m_tileShader = MeshManager::GetShader<CShader>("toonshader");
	if (m_tileShader == nullptr) {
		DBG_ERROR("   [FATAL] Shader 'toonshader' is NULL!");
	}
}

void GameScene::LoadRenderResources() {
	// 1. シェーダー（名前・VS・PS のテーブル駆動）
	struct ShaderDef { const char* name; const char* vs; const char* ps; };
	const ShaderDef shaderDefs[] = {
		{"toonshader",    "shader/ToonVS.hlsl",    "shader/ToonPS.hlsl"},
		{"outlineshader", "shader/OutlineVS.hlsl", "shader/OutlinePS.hlsl"},
		{"blobshader",    "shader/BlobVS.hlsl",    "shader/BlobPS.hlsl"},
		{"fxshader",      "shader/UnlitTextureVS.hlsl", "shader/UnlitTexturePS.hlsl"},
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
				m.Diffuse = ESCAPE_CUBE_COLOR;
				m.TextureEnable = FALSE;
				mat->SetMaterial(m);
			}
		});

	// 4. 2Dスプライト・UI画像
	m_escapeMarkerSprite = std::make_unique<CSprite>(ESCAPE_MARKER_TEX_SIZE, ESCAPE_MARKER_TEX_SIZE, "Assets/texture/ui/escape_marker.png");
	m_winTextSprite = std::make_unique<CSprite>(WIN_TEXT_TEX_W, WIN_TEXT_TEX_H, "Assets/texture/ui/win_text.png");
}

void GameScene::InitializeMap() {
	m_mapManager->SetScene(this);
	m_mapManager->Init(m_context);
	DBG_ERROR("   [GameScene] MapManager OK.");
	m_mapManager->LoadLevel("Assets/level/level_01.csv", m_context);

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
	m_playerController = std::make_unique<PlayerController>(m_context);
}

void GameScene::SetupUserInterface() {
	m_turnCutin = std::make_unique<TurnCutin>();
	m_turnCutin->Init();

	// ターン進行とカメラ演出のデカップリング（分離設計）
	m_sceneTurnConnection = m_context->GetTurnManager()->RegisterObserver([this](TurnState state) {
		if (state == TurnState::PlayerPhase) {
			if (m_turnCounter) m_turnCounter->Hide();

			if (m_isEscapeActive && m_ally && !m_ally->IsEscapeDone()) {
				// 脱出：即座に UI を再生し、Ally を注視
				m_turnCutin->PlayCutinAnimation("Player Phase");
				m_needsTurnCounterAnim = true;
				if (m_player) m_player->SetMenuHold(false);
				m_camera->ChangeState(CameraState::TargetFocus, m_ally->GetSRT().pos);
			}
			else if (m_remainingTurns != INITIAL_TURN_COUNT) {
				// 通常ターン：戦略視点へ戻る（前の対象を注視）→ 帰還後に UI 再生 → UI 終了後にプレイヤーへ急降下
				m_pendingCutinLabel = "Player Phase";
				m_pendingTurnCutin = true;
				m_pendingIsPlayerPhase = true;
				m_playerIntroPendingDive = true;
				if (m_camera) m_camera->HomeToStrategy();     // 注視点を前の対象に固定したまま、戦略視点へ戻る
				if (m_player) m_player->SetMenuHold(true);     // 一連の演出中はメニュー操作を無効化
			}
			else {
				// 1ターン目：IntroDirector に任せ、即座に UI を再生
				m_turnCutin->PlayCutinAnimation("Player Phase");
				m_needsTurnCounterAnim = true;
				if (m_player) m_player->SetMenuHold(false);
			}
		}
		else if (state == TurnState::EnemyPhase) {
			m_context->GetEnemyManager()->StartEnemyPhase();
			// まずカメラを戦略視点へ戻す（EnemyManager の CUT_HOME で制御）。カメラが所定位置に戻ってからカットインを再生
			m_pendingCutinLabel = "Enemy Phase";
			m_pendingTurnCutin = true;
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

void GameScene::MaybePlayPendingTurnCutin() {
	if (!m_pendingTurnCutin || !m_camera) return;
	if (m_camera->IsCinematic()) return;

	// カメラが完全に戦略俯瞰へ戻ってから、このターンの UI を再生
	if (m_camera->GetViewMode() != ViewMode::Strategy || !m_camera->IsAtTarget()) return;

	if (m_turnCutin) m_turnCutin->PlayCutinAnimation(m_pendingCutinLabel);
	m_pendingTurnCutin = false;

	if (m_pendingIsPlayerPhase) { // プレイヤーターン：カットイン直後にカウンターアニメーションを開始
		m_needsTurnCounterAnim = true;
		m_pendingIsPlayerPhase = false;
	}
}

void GameScene::InitializeDebugFeatures() {
	DBG_ERROR("   [GameScene] Registering DebugUI...");

	m_debugUI = std::make_unique<GameSceneDebugUI>(*this);
	DebugUI::RegisterDebugFunction([this]() { m_debugUI->Draw(); });
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
	if (!m_camera) return;

	// 追従対象が既に破棄されていたら参照を切る（復帰滑走中に消滅するケース）
	if (m_killCamTrackTarget && m_context && m_context->GetEnemyManager() &&
		!m_context->GetEnemyManager()->Contains(m_killCamTrackTarget)) {
		m_killCamTrackTarget = nullptr;
	}

	if (m_camera->IsCinematic()) {
		// 追従対象は演出開始時に一度だけ取得し、演出中は変更しない。
		// 複数の敵が連続で撃破されても、追従対象は切り替えない。
		if (!m_killCamTargetAcquired && m_context && m_context->GetEnemyManager()) {
			m_killCamTrackTarget = m_context->GetEnemyManager()->GetDyingEnemy();
			if (m_killCamTrackTarget) m_killCamTargetAcquired = true;
		}
		// 対象が破棄された場合は追従を停止し、現在の注視点を維持する。
		// 演出終了後は通常のカメラ制御へ復帰。
		if (m_killCamTrackTarget) {
			if (m_killCamTrackTarget->IsDeathVisualHidden()) {
				// 十字スター出現後：下方向への追従を止め、スター位置を注視したまま定格。
				// スターの寿命が尽きたら演出を早期終了してカメラを復帰させる。
				// （スターの寿命もこのタイマーも実時間なので同期が取れる）
				m_killCamStarTimer += deltaSeconds;
				if (m_killCamStarTimer >= Fx::Star.life) {
					m_camera->EndKillCam();
				}
			}
			else {
				m_killCamStarTimer = 0.0f;
				m_camera->UpdateKillCamFollow(m_killCamTrackTarget->GetSRT().pos);
			}
		}
	}
	else {
		// 演出終了後に追従対象をリセット（次回演出で再取得）
		m_killCamTrackTarget = nullptr;
		m_killCamTargetAcquired = false;
		m_killCamStarTimer = 0.0f;
	}

	// ===== バトルカメラ：定常時の構図（遷移中は EnemyManager の制御処理 / actorIntro が担当）=====
	float playerFade = 0.0f;
	TurnManager* tm = m_context ? m_context->GetTurnManager() : nullptr;
	bool playerPhase = tm && tm->GetTurnState() == TurnState::PlayerPhase;

	if (playerPhase) {
		if (m_camera->GetViewMode() == ViewMode::Battle) {
			// バトル視点へ移行した初フレーム：プレイヤーの背後に合わせる（一度だけ）。以降はマウス操作に委ねる
			if (!m_playerBattleOriented && m_player) {
				m_camera->OrientBehind(m_player->GetSRT().rot.y);
				m_playerBattleOriented = true;
			}

			// プレイヤーが操作可能な状態（メニュー表示中）のみマウス旋回を許可。
			// 急降下・待機中はカメラを回転させず、IsAtTarget が確実に収束するようにする
			PlayerState ps = m_player ? m_player->GetState() : PlayerState::WAITING;
			bool controllable = (ps != PlayerState::WAITING && ps != PlayerState::DEAD_FLYING);

			// マウス旋回：通常時は自由に操作。ImGui がマウスを使用している場合は右ボタンが必要
			auto& in = CDirectInput::GetInstance();
			bool imguiMouse = ImGui::GetIO().WantCaptureMouse;
			bool canOrbit = imguiMouse ? in.GetMouseRButtonCheck() : true;
			if (canOrbit) {
				float dx = (float)in.GetMouseMoveX();
				float dy = (float)in.GetMouseMoveY();
				if (dx != 0.0f || dy != 0.0f)
					m_camera->OrbitByMouse(dx * Camera::MOUSE_ORBIT_SENS_X, dy * Camera::MOUSE_ORBIT_SENS_Y);
			}

			// 接近によるフェード
			float eff = m_camera->GetEffectiveDistance();
			float span = Camera::PLAYER_FADE_START - Camera::PLAYER_FADE_FULL;
			if (span > 0.0001f) {
				float f = (Camera::PLAYER_FADE_START - eff) / span;
				playerFade = (f < 0.0f) ? 0.0f : (f > 1.0f ? 1.0f : f);
			}
		}
		else {
			// 遷移中にバトル視点を離れた場合 → リセットし、dive 完了後に再び背後へ合わせる
			m_playerBattleOriented = false;
		}
	}
	else {
		m_playerBattleOriented = false;

		// 敵ターン：CUT_DIVE / ACTING（GetActingEnemy が有効）のみ
		// 「プレイヤー側から敵を見る」構図を適用する。
		// CUT_HOME / CUT_SNAP 中は対象なし → カメラを HomeToStrategy / SnapLookAt に委ねる。
		Enemy* actor = (m_context && m_context->GetEnemyManager())
			? m_context->GetEnemyManager()->GetActingEnemy() : nullptr;

		if (actor && m_player)
			m_camera->FrameEnemyFromPlayer(m_player->GetSRT().pos, actor->GetSRT().pos);
	}

	m_playerFadeProximity = playerFade;
	m_camera->Update(deltaSeconds);
}

void GameScene::UpdateOcclusionFade()
{
	// --- 構造物（家具・壁）による遮蔽：前フレームの状態をリセット ---
	for (MapObject* o : m_occluders) if (o) o->SetOccluded(false);
	m_occluders.clear();

	// --- ユニット：関連するすべてのユニットの目標 fade をリセット ---
	EnemyManager* em = m_context ? m_context->GetEnemyManager() : nullptr;
	if (em) for (Enemy* e : em->GetAllEnemies()) if (e) e->SetTargetFade(0.0f);

	// プレイヤー：接近によるフェードのみ適用
	// （敵ターンでは前景にいるプレイヤーを遮蔽によって非表示にしない）
	if (m_player) m_player->SetTargetFade(m_playerFadeProximity);

	if (!m_camera || m_camera->GetViewMode() != ViewMode::Battle) return;

	Vector3 from = m_camera->GetLookat();     // 注視対象（主体）
	Vector3 to = m_camera->GetPosition();   // カメラ

	// --- 構造物による遮蔽（レイ上の遮蔽物を収集、既存の処理を使用）---
	if (m_mapManager) {
		Vector3 d = to - from;
		float dist = d.Length();
		if (dist > 0.001f) {
			d *= 1.0f / dist;
			m_mapManager->CollectOccluders(from, d, dist, m_occluders);
			for (MapObject* o : m_occluders) if (o) o->SetOccluded(true);
		}
	}

	// --- 敵による遮蔽：カメラと注視対象の間にいる敵をフェードアウト
	//     （注視対象自身はフェードさせない）---
	TurnManager* tm = m_context ? m_context->GetTurnManager() : nullptr;
	bool playerPhase = tm && tm->GetTurnState() == TurnState::PlayerPhase;
	Unit* subject = playerPhase ? (Unit*)m_player
		: (Unit*)(em ? em->GetActingEnemy() : nullptr);

	// XZ 平面：点 p が線分 from → to の中間付近にあるかを判定
	// （視線を遮っているかどうかを判定）
	auto occludes = [&](const Vector3& p) -> bool {
		float ax = from.x, az = from.z, bx = to.x, bz = to.z;
		float abx = bx - ax, abz = bz - az;
		float ab2 = abx * abx + abz * abz;
		if (ab2 < 1e-4f) return false;

		float t = ((p.x - ax) * abx + (p.z - az) * abz) / ab2;
		if (t < 0.05f || t > 0.95f) return false;              // 主体またはカメラに近すぎる場合は遮蔽とみなさない

		float cx = ax + abx * t, cz = az + abz * t;
		float dx = p.x - cx, dz = p.z - cz;
		return (dx * dx + dz * dz) < UNIT_OCCLUDE_RADIUS * UNIT_OCCLUDE_RADIUS;
		};

	if (em) {
		for (Enemy* e : em->GetAllEnemies()) {
			if (!e || e == subject || e->IsDead()) continue;
			if (occludes(e->GetSRT().pos)) e->SetTargetFade(1.0f);
		}
	}
}

void GameScene::UpdateTurnIntroSequence(float deltaSeconds) {
	if (m_needsTurnCounterAnim) {
		if (m_turnCounter) m_turnCounter->StartAnimation();
		m_needsTurnCounterAnim = false;

		if (m_remainingTurns == INITIAL_TURN_COUNT && m_introDirector && m_introDirector->IsIdle()) {
			m_introDirector->Start();
		}
	}

	if (m_turnCounter) m_turnCounter->Update(deltaSeconds);
	if (m_introDirector) m_introDirector->Update(deltaSeconds, m_isAllyTalked);

	// プレイヤーターン：カットイン＋カウンターの再生がすべて終了した後、プレイヤーへ急降下
	if (m_playerIntroPendingDive && !m_pendingTurnCutin
		&& m_turnCutin && !m_turnCutin->IsAnimating()
		&& m_turnCounter && !m_turnCounter->IsAnimating()
		&& m_camera && !m_camera->IsCinematic()) {

		if (m_player) {
			m_camera->BeginActorTransition(m_player->GetSRT().pos, m_player->GetSRT().rot.y);
			m_player->SetMenuHold(false);   // 以降はカメラ側（IsActorTransitioning + IsAtTarget）で操作を制御
		}

		m_playerIntroPendingDive = false;
	}
}


// ---------------------------------------------------------
 
// 更新サブルーチン：フロー制御インターセプト (Flow Control)

// ---------------------------------------------------------
bool GameScene::HandlePreGameBlocking(float deltaSeconds) {
	if (m_isGameStarted) return false;

	m_startDelayTimer += deltaSeconds;

	// 画面遷移直後の違和感を防ぐため、一定時間待機してから進行を開始
	if (m_startDelayTimer >= START_WAIT_TIME) {
		if (m_tutorialUI && !m_tutorialUI->IsAllFinished()) {
			m_tutorialUI->Update(deltaSeconds);
			if (m_background) m_background->Update(deltaSeconds);
			return true;
		}

		m_isGameStarted = true;
		if (m_camera) m_camera->SetTargetRadius(Camera::BASE_RADIUS);

		if (m_context && m_context->GetTurnManager()) {
			DBG_ERROR("[GameScene] FadeIn Complete. Start Player Phase!");
			m_context->GetTurnManager()->SetState(TurnState::PlayerPhase);
		}
	}

	if (m_background) m_background->Update(deltaSeconds);
	return true;
}

bool GameScene::HandleTurnCutinBlocking(float deltaSeconds)
{
	// ターン切り替え時の重要な演出：他の動きをすべて止め、プレイヤーの視線をカットインに集中させる
	if (m_turnCutin && m_turnCutin->IsAnimating()) {
		m_turnCutin->Update(deltaSeconds);
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

void GameScene::UpdateEnvironmentAndDamageUI(float deltaSeconds)
{
	if (m_background) m_background->Update(deltaSeconds);
	if (m_damageNumberManager) m_damageNumberManager->Update(deltaSeconds);
}

void GameScene::HandleCameraRotationInput()
{
	//// リザルト演出中やシーン遷移中は、カメラの不自然な回転を防ぐため入力を無視する
	//bool canRotate = (m_isGameStarted && m_resultJudge &&
	//	!m_resultJudge->IsGameOverProcessing() && !m_resultJudge->IsSceneChanging());

	if (m_gameUIManager) {
		m_gameUIManager->SetCameraRotateVisible(false);
	}

	// Debug：F2 で 三人称 <-> 战略 を切替
	if (CDirectInput::GetInstance().CheckKeyBufferTrigger(DIK_F2)) {
		ToggleViewModeDebug();
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
			m_camera->ChangeState(CameraState::TargetFocus, m_player->GetSRT().pos);
		}

		m_player->StartCelebration();
	}
}

void GameScene::UpdateGameObjects(float deltaSeconds)
{
	// プレイヤーが有効な間、最新の操作コマンドを渡す。
	// コマンドの処理可否は Player のステートマシン側で制御する。
	if (m_player && m_playerController) {
		if (m_player->GetState() != PlayerState::DEAD_FLYING) {
			m_player->SetCommand(m_playerController->PollInput());
		}
	}

	for (const auto& obj : m_gameObjectList) {
		obj->Update(deltaSeconds);
	}

	if (m_context && m_context->GetEnemyManager()) {
		m_context->GetEnemyManager()->Update(deltaSeconds);
	}
}

void GameScene::ProcessAllyTacticalDialogue()
{
	if (!m_context || !m_context->GetTurnManager()) return;

	if (m_context->GetTurnManager()->GetTurnState() == TurnState::PlayerPhase) {
		if (m_turnCutin && !m_turnCutin->IsAnimating()) {
			if (!m_isAllyTalked) {
				// 戦術の誘導：プレイヤーターンの開始時、生存している味方から行動のヒントを提示
				if (m_ally && m_ally->GetHP() > 0 && !m_ally->IsEscaping()) {
					Vector3 allyPos = m_ally->GetSRT().pos;
					if (m_dialogueUI) {
						// 導入演出中は吹き出しを長めに表示（IntroDirector は吹き出しが閉じるまでカメラを戻さない）
						float duration = (m_introDirector && !m_introDirector->IsFinished())
							? INTRO_ALLY_HELP_DURATION : ALLY_HELP_DURATION;
						m_dialogueUI->ShowDialogue(allyPos, DialogueType::Help, duration);
					}
				}
				m_isAllyTalked = true;
			}
		}
	}
}

void GameScene::UpdatePostEffectsAndAudio(float deltaSeconds)
{
	if (m_context && m_context->GetDialogueUI()) {
		m_context->GetDialogueUI()->Update(deltaSeconds);
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
	--m_remainingTurns;
	if (m_remainingTurns < 0) {
		m_remainingTurns = 0; // 負の値にならないようクランプ（防御的プログラミング）
	}

	// UIの更新
	if (m_turnCounter) m_turnCounter->SetTurn(m_remainingTurns);

	// 脱出予約：規定ターン到達後、次のプレイヤーフェーズで
		// 「採掘 → 脱出点出現 → 台詞 → 消失」の順に Ally 側で演出する
	if (m_remainingTurns <= 0 && !m_isEscapeActive) {
		m_isEscapeActive = true;
		if (m_ally) {
			m_escapeGridX = m_ally->GetUnitGridX();
			m_escapeGridZ = m_ally->GetUnitGridZ();
			m_ally->ArmEscape();
		}
	}

	// 味方が脱出後、味方の会話フラグをリセット
	m_isAllyTalked = false;

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

void GameScene::DrawFloorLayer(float deltaSeconds) {
	for (const auto& obj : m_gameObjectList) {
		MapObject* mapObj = obj->AsMapObject();
		if (mapObj && mapObj->GetType() == MapModelType::FLOOR) {
			obj->Draw(deltaSeconds);
		}
	}
}

void GameScene::DrawFloorUIHints(float deltaSeconds) {
	Renderer::SetBlendState(BS_ALPHABLEND);
	for (const auto& obj : m_gameObjectList) {
		obj->DrawFloorUI(deltaSeconds);
	}
	Renderer::SetBlendState(BS_NONE);
	if (m_tileShader) m_tileShader->SetGPU(); 
}

void GameScene::DrawEnvironmentAndEntities(float deltaSeconds) {
	// === 1. 床面特殊オブジェクトレイヤー (Trap) ===
	
	// 先に描画した床面UI（半透明）の上に確実に乗せるため、ここで描画する
	for (const auto& obj : m_gameObjectList) {
		MapObject* mapObj = dynamic_cast<MapObject*>(obj.get());
		if (mapObj && mapObj->GetType() == MapModelType::TRAP) {
			obj->Draw(deltaSeconds);
		}
	}

	// === 2. 実体エンティティレイヤー (Props, Walls, Characters) ===
	for (const auto& obj : m_gameObjectList) {
		MapObject* mapObj = dynamic_cast<MapObject*>(obj.get());
		if (mapObj) {
			// 床とトラップ以外のマップオブジェクトを描画
			if (mapObj->GetType() != MapModelType::FLOOR && mapObj->GetType() != MapModelType::TRAP) {
				obj->Draw(deltaSeconds);
			}
		}
		else {
			// キャラクター（Player, Enemy, Ally）を描画
			obj->Draw(deltaSeconds);
		}
	}
}

void GameScene::DrawTransparentWorld(float deltaSeconds) {
	Renderer::SetBlendState(BS_ALPHABLEND);

	// 半透明エンティティ（残像など）
	for (const auto& obj : m_gameObjectList) {
		obj->DrawTransparent(deltaSeconds);
	}

	// 仲間の脱出（採掘・フェードアウト）が完了した後のみ、空色のマスを描画
	bool shouldDrawEscape = (m_isEscapeActive && m_ally && m_ally->IsEscapePointVisible()) || m_shouldShowDebugEscape;
	if (shouldDrawEscape) {
		DrawEscapeCube();
	}

	Renderer::SetBlendState(BS_NONE);

	if (m_context && m_context->GetEffectManager()) m_context->GetEffectManager()->Draw3D();
}

void GameScene::DrawTacticalOverlays(float deltaSeconds) {
	// 戦術オーバーレイ：壁や他のキャラクターに隠れて見えなくなるのを防ぐため、深度計算をスキップする
	Renderer::SetDepthEnable(false);
	Renderer::SetBlendState(BS_ALPHABLEND);

	for (const auto& obj : m_gameObjectList) {
		obj->DrawOverlay(deltaSeconds);   // 浮遊矢印、ヒット警告エフェクトなど
	}

	Renderer::SetBlendState(BS_NONE);
	Renderer::SetDepthEnable(true);
}

void GameScene::DrawDamageAndHitEffects() {

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
	// EnemyManager が管理する敵リストを直接使用（全リスト走査 + RTTI を回避）
	if (m_context && m_context->GetEnemyManager()) {
		for (Enemy* enemy : m_context->GetEnemyManager()->GetAllEnemies()) {
			if (enemy && !enemy->IsDead()) enemy->DrawUI();
		}
	}
	if (m_dialogueUI) m_dialogueUI->Draw();

	// --- 2. 特定状況下のポップアップ ---
	bool shouldDrawEscape = (m_isEscapeActive && m_ally && m_ally->IsEscapePointVisible()) || m_shouldShowDebugEscape;
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
	if (!m_mapManager || !m_camera) return;

	const auto& allTiles = m_mapManager->GetAllTiles();
	if (!allTiles.empty()) {
		float minX = FLT_MAX, maxX = -FLT_MAX;
		float minZ = FLT_MAX, maxZ = -FLT_MAX;

		for (const auto& tile : allTiles) {
			Vector3 pos = m_mapManager->GetWorldPosition(tile);
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
	CStaticMeshRenderer* escapeCube = MeshManager::GetRenderer<CStaticMeshRenderer>("escape_cube_mesh");
	if (escapeCube) {
		Vector3 pos = m_context->GetMapManager()->GetWorldPosition(m_escapeGridX, m_escapeGridZ);
		pos.y += ESCAPE_CUBE_Y_OFFSET;
		Matrix4x4 world = Matrix4x4::CreateTranslation(pos);
		Renderer::SetWorldMatrix(&world);
		escapeCube->Draw();
	}
}

void GameScene::DrawEscapeMarker() {
	if (!m_escapeMarkerSprite) return;
	Vector3 worldPos = m_context->GetMapManager()->GetWorldPosition(m_escapeGridX, m_escapeGridZ);

	// === 正弦波を利用して上下の浮遊感を計算 ===
	float bobbingOffset = sinf(m_uiAnimTimer * ESCAPE_MARKER_FLOAT_SPEED) * ESCAPE_MARKER_FLOAT_AMPLITUDE;

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
	basePos.y += WIN_TEXT_Y_OFFSET;

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

//Debugカメラ遷移
void GameScene::ToggleViewModeDebug()
{
	if (!m_camera || !m_player) return;
	if (m_camera->GetViewMode() == ViewMode::Battle) {
		m_camera->EnterStrategyView();
	}
	else {
		m_camera->BeginActorTransition(m_player->GetSRT().pos, m_player->GetSRT().rot.y);
	}
}



