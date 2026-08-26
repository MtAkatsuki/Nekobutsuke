#pragma once

#include <memory>
#include "../../System/IScene.h"
#include "../../System/SceneClassFactory.h"
#include "../../GamePlay/Manager/TurnManager.h"
#include "../../Actor/Character/PlayerController.h"

class GameContext;
class Camera;
class MapManager;
class GameUIManager;
class DamageNumberManager;
class DialogueUI;
class GameSceneDebugUI;
class IntroDirector;
class GameResultJudge;
class Enemy;
class Player;
class Ally;
class Background;
class TurnCutin;
class TurnCounter;
class TutorialUI;
class CSprite;
class CShader;

// =========================================================
// GameScene クラス
// 戦術シミュレーションのメインループと状態管理を担うシーン
// =========================================================
class GameScene : public IScene {
public:

	virtual ~GameScene();
	friend class GameSceneDebugUI;   // デバッグ UI に内部状態(調参対象)への直接アクセスを許可

	// コピーコンストラクタ・代入演算子の無効化（安全設計）
	GameScene(const GameScene&) = delete;
	GameScene& operator=(const GameScene&) = delete;

	explicit GameScene();

	// ---------------------------------------------------------
	// IScene 継承ライフサイクル (Lifecycle Overrides)
	// ---------------------------------------------------------
	void Init() override;
	void Update(float deltaSeconds) override;
	void Draw(float deltaSeconds) override;
	void Dispose() override;

	// ---------------------------------------------------------
	// 外部インターフェース・初期設定 (Public Interfaces)
	// ---------------------------------------------------------
	void SetGameContext(GameContext* context) override;

	void AddObject(std::unique_ptr<GameObject> obj) {
		m_gameObjectList.push_back(std::move(obj));
	}
	// ---------------------------------------------------------
	// デバグ機能 (Debug)
	// ---------------------------------------------------------
	void ToggleViewModeDebug();

	// 初期ターン数（脱出イベントまでのカウントダウン開始値）
	static constexpr int INITIAL_TURN_COUNT = 5;

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
	void LoadRenderResources();
	void InitializeMap();
	void SetupGameEntities();
	void SetupUserInterface();
	void InitializeDebugFeatures();

	// ---------------------------------------------------------
	// 更新サブルーチン：システム・表現 (System & Presentation)
	// ---------------------------------------------------------
	void UpdateCoreTimers(float deltaSeconds);
	void UpdateCameraFocus(float deltaSeconds);
	void UpdateTurnIntroSequence(float deltaSeconds);
	void UpdateOcclusionFade(); // 毎フレーム、カメラと本体の間にある遮蔽物を検出し、その fade 値を制御
	// ---------------------------------------------------------
	// 更新サブルーチン：フロー制御インターセプト (Flow Control Interceptors)
	// ---------------------------------------------------------
	// 開始前の待機・チュートリアル進行を処理。まだ本編を止めるべきなら true
	bool HandlePreGameBlocking(float deltaSeconds);
	// ターンカットイン再生中は true を返し、後続の更新を止める
	bool HandleTurnCutinBlocking(float deltaSeconds);
	// ターンカウンター演出中か（盤面更新を一時停止するかの判定）
	bool IsTurnCounterAnimating() const;

	// ---------------------------------------------------------
	// 更新サブルーチン：メインロジック (Main Logic & Entities)
	// ---------------------------------------------------------
	void UpdateEnvironmentAndDamageUI(float deltaSeconds);
	void HandleCameraRotationInput();
	void ProcessEscapeEvent();
	void UpdateGameObjects(float deltaSeconds);
	void ProcessAllyTacticalDialogue();
	void UpdatePostEffectsAndAudio(float deltaSeconds);
	void RemoveDeadObjects();

	// ---------------------------------------------------------
	// ターン進行とイベント制御 (Turn Flow & Events)
	// ---------------------------------------------------------
	void TurnChangeCheck();
	void ProcessEndOfEnemyPhase();
	void MaybePlayPendingTurnCutin();   // カメラが所定位置に戻った後に、このターンのカットインを再生

	// ---------------------------------------------------------
	// レンダリングパイプライン (Rendering Sub-routines)
	// ---------------------------------------------------------
	void DrawBackgroundLayer();
	void DrawFloorLayer(float deltaSeconds);
	void DrawFloorUIHints(float deltaSeconds);
	void DrawEnvironmentAndEntities(float deltaSeconds);
	void DrawTransparentWorld(float deltaSeconds);
	void DrawTacticalOverlays(float deltaSeconds);
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
	MapManager* m_mapManager = nullptr;
	GameUIManager* m_gameUIManager = nullptr;
	DamageNumberManager* m_damageNumberManager = nullptr;
	CShader* m_tileShader = nullptr;
	TurnManager::ScopedConnection m_sceneTurnConnection;

	// --- ゲームエンティティ ---
	std::vector<std::unique_ptr<GameObject>> m_gameObjectList;
	Player* m_player = nullptr;
	Ally* m_ally = nullptr;
	std::unique_ptr<Background> m_background;
	std::unique_ptr<PlayerController> m_playerController;

	// --- UIコンポーネント ---
	DialogueUI* m_dialogueUI = nullptr;
	std::unique_ptr<TurnCutin> m_turnCutin;
	std::unique_ptr<TurnCounter> m_turnCounter;
	std::unique_ptr<TutorialUI> m_tutorialUI;
	std::unique_ptr<CSprite> m_escapeMarkerSprite;
	std::unique_ptr<CSprite> m_winTextSprite;
	std::unique_ptr<GameSceneDebugUI> m_debugUI;
	std::unique_ptr<GameResultJudge> m_resultJudge;
	std::unique_ptr<IntroDirector> m_introDirector;
	bool m_showActionUI = true;

	// --- ゲーム進行状態フラグ ---
	bool m_isGameStarted = false;
	bool m_isEscapeActive = false;
	bool m_isAllyTalked = false;

	// --- パラメータ・タイマー ---
	int m_remainingTurns = INITIAL_TURN_COUNT;
	int m_escapeGridX = -1;
	int m_escapeGridZ = -1;

	float m_uiAnimTimer = 0.0f;
	float m_startDelayTimer = 0.0f;

	// --- 演出・デバッグ制御 ---
	bool m_needsTurnCounterAnim = false;
	bool m_shouldShowDebugEscape = false;
	Enemy* m_killCamTrackTarget = nullptr;// KillCam の追従対象（演出中は固定）
	bool m_killCamTargetAcquired = false; // 本演出で追従対象を取得済みか（再取得を行わない）
	float m_killCamStarTimer = 0.0f;      // 十字スター開始からの経過（実時間、終了で復帰）
	bool m_playerBattleOriented = false;   // プレイヤーがバトルモードに入った際、背後方向へ一度だけ向きを合わせる
	bool        m_pendingTurnCutin = false;
	const char* m_pendingCutinLabel = "";
	bool m_pendingIsPlayerPhase = false;   // カメラ帰還後にカットインを再生する際、カウンターも同時に更新するか
	bool m_playerIntroPendingDive = false;   // UI 終了後にプレイヤー視点への急降下を待機中

	// --- 定数パラメータ ---
	const float START_WAIT_TIME = 1.0f;
	// --- Dither Fade ---
	std::vector<class MapObject*> m_occluders; // 今フレームの遮蔽物（次フレームでリセット）
	float m_playerFadeProximity = 0.0f;   // カメラの接近によるプレイヤーのフェード量（UpdateCameraFocus で算出）
};

REGISTER_CLASS(GameScene)