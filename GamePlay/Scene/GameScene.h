#pragma once

#include <array>
#include <memory>

#include "../../System/IScene.h"
#include "../../System/SceneClassFactory.h"
#include "../../System/DirectWrite.h"
#include "../../System/RandomEngine.h"
#include "../../Actor/Character/Enemy.h"
#include "../../Actor/Character/Player.h"
#include "../../Actor/Character/Ally.h"
#include "../../UI/Component/TurnCutin.h"
#include "../../UI/Component/TurnCounter.h"
#include "../../UI/Component/TutorialUI.h"
#include "Background.h"

class GameContext;
class Camera;
class MapManager;
class TurnManager;
class GameUIManager;
class DamageNumberManager;
class DialogueUI;

// =========================================================
// ゲーム開始時やターン開始時の導入シネマティック状態
// =========================================================
enum class IntroState {
	Idle,                // 待機状態
	TurnCounterFlying,   // ターンカウントUIのポップアップ・移動中
	CameraToAlly,        // カメラが味方（ネズミ）へスムーズに移動
	WaitingAllyDialogue, // カメラが到着し、味方のセリフ演出が完了するのを待機
	CameraToBase,        // カメラが一度全体俯瞰（BaseView）へ戻る
	CameraToPlayer,      // カメラがプレイヤーへ戻る
	Finished             // すべての導入演出が完了
};

// =========================================================
// GameScene クラス
// 戦術シミュレーションのメインループと状態管理を担うシーン
// =========================================================
class GameScene : public IScene {
public:
	static constexpr uint32_t ENEMYMAX = 3;

	virtual ~GameScene() {}

	// コピーコンストラクタ・代入演算子の無効化（安全設計）
	GameScene(const GameScene&) = delete;
	GameScene& operator=(const GameScene&) = delete;

	explicit GameScene();

	// ---------------------------------------------------------
	// IScene 継承ライフサイクル (Lifecycle Overrides)
	// ---------------------------------------------------------
	void Init() override;
	void update(uint64_t deltatime) override;
	void draw(uint64_t deltatime) override;
	void dispose() override;

	// ---------------------------------------------------------
	// 外部インターフェース・初期設定 (Public Interfaces)
	// ---------------------------------------------------------
	void SetGameContext(GameContext* context) override;

	void AddObject(std::unique_ptr<GameObject> obj) {
		m_GameObjectList.push_back(std::move(obj));
	}

	// ---------------------------------------------------------
	// デバッグ及びツール用関数 (Debug & Tools)
	// ---------------------------------------------------------
	void debugUICamera();
	void drawGridDebugText();
	Enemy* SpawnDebugEnemyInFront(int hp = -1);

private:
	// =========================================================
	// 内部ロジック分割サブ関数群 (Cataloging)
	// 読み手が更新順序と意図を俯瞰できるように役割ごとに分割
	// =========================================================

	// ---------------------------------------------------------
	// 初期化サブルーチン (Init Sub-routines)
	// ---------------------------------------------------------
	void ResetManagers();
	void InitializeCamera();
	void LoadGameResources();
	void resourceLoader();
	void InitializeMap();
	void SetupGameEntities();
	void SetupUserInterface();
	void InitializeDebugFeatures();

	// ---------------------------------------------------------
	// 更新サブルーチン：システム・表現 (System & Presentation)
	// ---------------------------------------------------------
	void UpdateCoreTimers(float deltaSeconds);
	void UpdateCameraFocus(float deltaSeconds);
	void UpdateTurnIntroSequence(uint64_t deltatime, float deltaSeconds);
	void UpdateIntroSequence(float deltaSeconds);

	// ---------------------------------------------------------
	// 更新サブルーチン：フロー制御インターセプト (Flow Control Interceptors)
	// ---------------------------------------------------------
	bool HandlePreGameBlocking(uint64_t deltatime, float deltaSeconds);
	bool HandleTurnCutinBlocking(uint64_t deltatime);
	bool IsTurnCounterAnimating() const;

	// ---------------------------------------------------------
	// 更新サブルーチン：メインロジック (Main Logic & Entities)
	// ---------------------------------------------------------
	void UpdateEnvironmentAndDamageUI(uint64_t deltatime);
	void HandleCameraRotationInput();
	void ProcessEscapeEvent();
	void UpdateGameObjects(uint64_t deltatime);
	void ProcessAllyTacticalDialogue();
	void UpdatePostEffectsAndAudio(uint64_t deltatime, float deltaSeconds);

	// ---------------------------------------------------------
	// 勝敗判定とシーン遷移 (Game Status & Transitions)
	// ---------------------------------------------------------
	void CheckGameStatus(float deltaSeconds);
	bool CheckGameOverCondition() const;
	bool ProcessGameOverFlow(float deltaSeconds);
	bool CheckGameClearCondition() const;
	bool IsFieldBusyForClear() const;
	void ProcessGameClearFlow(float deltaSeconds);

	// ---------------------------------------------------------
	// ターン進行とイベント制御 (Turn Flow & Events)
	// ---------------------------------------------------------
	void TurnChangeCheck();
	void ProcessEndOfEnemyPhase();
	void CheckAndTriggerEscapeEvent();

	// ---------------------------------------------------------
	// レンダリングパイプライン (Rendering Sub-routines)
	// ---------------------------------------------------------
	void DrawBackgroundLayer();
	void DrawFloorLayer(uint64_t deltatime);
	void DrawFloorUIHints(uint64_t deltatime);
	void DrawEnvironmentAndEntities(uint64_t deltatime);
	void DrawTransparentWorld(uint64_t deltatime);
	void DrawTacticalOverlays(uint64_t deltatime);
	void DrawDamageAndHitEffects();
	void DrawScreenSpaceUI();

	void RecalculateCameraBounds();
	void DrawEscapeCube();
	void DrawEscapeMarker();
	void DrawWinText();

	// =========================================================
	// メンバー変数 (Member Variables)
	// =========================================================

	// --- コアシステムとマネージャー ---
	GameContext* m_context = nullptr;
	Camera* m_camera = nullptr;
	MapManager* m_MapManager = nullptr;
	TurnManager* m_turnManager = nullptr;
	GameUIManager* m_gameUIManager = nullptr;
	DamageNumberManager* m_damageNumberManager = nullptr;
	CShader* m_tileShader = nullptr;
	std::unique_ptr<DirectWrite> m_directwrite;

	// --- ゲームエンティティ ---
	std::vector<std::unique_ptr<GameObject>> m_GameObjectList;
	std::array<Enemy*, ENEMYMAX> m_enemies;
	Enemy* m_debugEnemy = nullptr;// 直近のテスト用エネミー（次回スポーン時に掃除）
	Player* m_player = nullptr;
	Ally* m_ally = nullptr;
	std::unique_ptr<Background> m_background;

	// --- UIコンポーネント ---
	DialogueUI* m_dialogueUI = nullptr;
	std::unique_ptr<TurnCutin> m_turnCutin;
	std::unique_ptr<TurnCounter> m_turnCounter;
	std::unique_ptr<TutorialUI> m_tutorialUI;
	std::unique_ptr<CSprite> m_escapeMarkerSprite;
	std::unique_ptr<CSprite> m_winTextSprite;
	bool m_showActionUI = true;

	// --- ゲーム進行状態フラグ ---
	bool m_isGameStarted = false;
	bool m_isSceneChanging = false;
	bool m_isGameOverProcessing = false;
	bool m_isEscapeActive = false;
	bool m_isAllyTalked = false;

	// --- パラメータ・タイマー ---
	int m_remainingTurns = 5;
	int m_escapeGridX = -1;
	int m_escapeGridZ = -1;

	float m_uiAnimTimer = 0.0f;
	float m_startDelayTimer = 0.0f;
	float m_gameOverTimer = 0.0f;
	float m_gameClearTimer = 0.0f;
	float m_introTimer = 0.0f;

	// --- 演出・デバッグ制御 ---
	IntroState m_introState = IntroState::Idle;
	bool m_needsTurnCounterAnim = false;
	bool m_shouldShowDebugEscape = false;
	bool m_isDebugCameraEnabled = true;

	// --- 定数パラメータ ---
	const float START_WAIT_TIME = 1.0f;
	const float GAMEOVER_WAIT_DURATION = 1.0f;
	const float GAMECLEAR_WAIT_DURATION = 1.0f;

	// デバッグ用マウスピッキング座標
	Vector3 m_pickuppos{ 0,0,0 };
	Vector3 m_farpoint{};
	Vector3 m_nearpoint{};
};

REGISTER_CLASS(GameScene)