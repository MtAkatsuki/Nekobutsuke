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
#include "Background.h"

class GameContext;
class Camera;
class MapManager;
class TurnManager;
class GameUIManager;
class DamageNumberManager;
class DialogueUI;

//enum class CameraMode 
//{	
//	FPS,
//	TopDown
//};

enum class CameraDebugMode {
	Orbit,
	Free
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



	// 平行光源
	void debugDirectionalLight();


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


	//grassとwallのtexture
	std::unique_ptr<CTexture> m_grassTexture = nullptr;
	std::unique_ptr<CTexture> m_wallTexture = nullptr;
	std::unique_ptr<CTexture> m_blockTexture = nullptr;

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


	bool m_enableDebugCamera = true;


	CameraDebugMode m_cameraDebugMode = CameraDebugMode::Orbit;
	
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

	private:
		void TurnChangeCheck();


};

REGISTER_CLASS(GameScene)