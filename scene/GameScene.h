#pragma once

#include <array>
#include <memory>


#include "../system/IScene.h"
#include "../system/SceneClassFactory.h"
#include "../system/DirectWrite.h"
#include "../system/RandomEngine.h"
#include "../gameobject/enemy.h"
#include "../gameobject/player.h"
#include "../gameobject/Ally.h"
#include "../ui/TurnCutin.h"
#include "../ui/TurnCounter.h"
#include "../ui/TutorialUI.h"
#include "Background.h"

class GameContext;
class Camera;
class MapManager;
class TurnManager;
class GameUIManager;
class DamageNumberManager;
class DialogueUI;

//enum class CameraDebugMode {
//	Orbit,
//	Free
//};

// --- ゲーム開始時やターン開始時の導入シネマティック状態 ---
enum class IntroState {
	Idle,                // 待機状態
	TurnCounterFlying,   // ターンカウントUIのポップアップ・移動中
	CameraToAlly,        // カメラが味方（ネズミ）へスムーズに移動
	WaitingAllyDialogue, // カメラが到着し、味方のセリフ演出が完了するのを待機
	CameraToBase,        // 【新規】カメラが一度全体俯瞰（BaseView）へ戻る
	CameraToPlayer,      // カメラがプレイヤーへ戻る
	Finished             // すべての導入演出が完了
};

/**
 * @brief カメラ
 */
class GameScene : public IScene {
public:
	virtual ~GameScene() {}
	static constexpr uint32_t ENEMYMAX = 3;

	/// @brief コピーコンストラクタは使用不可
	GameScene(const GameScene&) = delete;

	/// @brief 代入演算子も使用不可
	GameScene& operator=(const GameScene&) = delete;

	/**
	 * @brief コンストラクタ
	 */
	explicit GameScene();

	/**
	 * @brief 毎フレームの更新処理
	 * @param deltatime 前フレームからの経過時間（マイクロ秒）
	 */
	void update(uint64_t deltatime) override;

	/**
	 * @brief 毎フレームの描画処理
	 * @param deltatime 前フレームからの経過時間（マイクロ秒）
	 *
	 */
	void draw(uint64_t deltatime) override;

	/**
	 * @brief シーンの初期化処理
	 *
	 */
	void Init() override;

	/**
	 * @brief シーンの終了処理
	 *
	 */
	void dispose() override;

	/**
	 * @brief カメラの設定
	 *
	 */
	void debugUICamera();

	void drawGridDebugText();



	// リソースを読み込む
	void resourceLoader();

	void AddObject(std::unique_ptr<GameObject> obj) {
		m_GameObjectList.push_back(std::move(obj));
	}

	void SetGameContext(GameContext* context) override;

	void CheckGameStatus(float deltaSeconds);


private:
	GameContext* m_context = nullptr;
	 /**
	 * @brief このシーンで使用するカメラ
	 */
	Camera* m_camera = nullptr;

	/**
	* @brief フィールド
	*/
	MapManager* m_MapManager = nullptr;
	//ターン管理
	TurnManager* m_turnManager = nullptr;
	//contextから「吹き出し」を取得
	DialogueUI* m_dialogueUI = nullptr;
	//UI管理
	GameUIManager* m_gameUIManager = nullptr;
	//ダメージ数字管理
	DamageNumberManager* m_damageNumberManager = nullptr;

	// ゲームオブジェクトリスト
	std::vector<std::unique_ptr<GameObject>> m_GameObjectList;

	// 敵
	std::array<Enemy*, ENEMYMAX> m_enemies;
	// プレイヤー
	Player* m_player = nullptr;
	//仲間
	Ally* m_ally = nullptr;

	// DirectWrite
	std::unique_ptr<DirectWrite> m_directwrite;

	// フォントデータ
	FontData	m_fontdata;


	// 脱出に関連する変数を追加：
	bool m_isEscapeActive = false;
	int m_escapeGridX = -1;
	int m_escapeGridZ = -1;
	std::unique_ptr<CSprite> m_escapeMarkerSprite; // 脱出地点の浮遊UI
	std::unique_ptr<CSprite> m_winTextSprite;      // 頭上の「WIN」UI

	CShader* m_tileShader = nullptr;

	//マウスでピックアップした位置
	//debugUICameraで使用
	Vector3 m_pickuppos{ 0,0,0 };
	Vector3 m_farpoint{};
	Vector3 m_nearpoint{};


	// ターンカットイン表示
	std::unique_ptr<TurnCutin> m_turnCutin;
	// ターンカウンター
	std::unique_ptr<TurnCounter> m_turnCounter;
	// チュートリアルUI
	std::unique_ptr<TutorialUI> m_tutorialUI;


	bool m_enableDebugCamera = true;


	//CameraDebugMode m_cameraDebugMode = CameraDebugMode::Orbit;
	
	// Scene変更中フラグ
	bool m_isSceneChanging = false;	

	float m_gameClearTimer = 0.0f;
	const float GAMECLEAR_WAIT_DURATION = 1.0f; // クリア後の待機時間
	//背景
	std::unique_ptr<Background> m_background;

	int m_remainingTurns = 5; // 残りターン数（勝利まで）
	bool m_isAllyTalked = false; //今の回のターンで仲間が話したかどうか
	//ゲームシーン遷移するとき、黒色のフラッシュしないためのメンバー
	bool m_isGameStarted = false; // ゲーム始めているか
	float m_startDelayTimer = 0.0f; 
	const float START_WAIT_TIME = 1.0f; // 待ち時間

	// ===敗北ロジック制御変数===
	// フラグ：敗北処理が開始されているか（リザルトへの遷移待ち状態）
	bool m_isGameOverProcessing = false;
	// 敗北後のウェイト用タイマー
	float m_gameOverTimer = 0.0f;
	// 敗北待ち時間（秒）：ダメージ表示やHPゲージの減少、プレイヤーの反応時間を考慮
	const float GAMEOVER_WAIT_DURATION = 1.0f;
	// デバッグ用：脱出ガイドの表示切替フラグ
	bool m_debugShowEscape = false;

	// uiアニメーションタイマー
	float m_uiAnimTimer = 0.0f;
	// ターンカウンターアニメーションの必要性フラグ
	bool m_needsTurnCounterAnim = false;

	IntroState m_introState = IntroState::Idle;
	float m_introTimer = 0.0f; // 演出用の汎用タイマーを追加

	private:
		void TurnChangeCheck();
		// ====== カメラ境界の再計算メソッド ======
		void RecalculateCameraBounds();
		// 脱出ガイド関連の描画メソッド
		void DrawEscapeCube();
		void DrawEscapeMarker();
		// 勝利ジャンプ関連の描画メソッド
		void DrawWinText();

		// 開幕のカメラ演出（オープニングシーケンス）の更新関数
		void UpdateIntroSequence(float deltaSeconds);

};

REGISTER_CLASS(GameScene)