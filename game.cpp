#include	"system/renderer.h"
#include    "system/DebugUI.h"
#include    "system/CDirectInput.h"
#include	"system/scenemanager.h"
#include	"fpscontrol.h"
#include    "game.h"
#include	"manager/GameContext.h"
#include	"system/FadeTransition.h"
#include    "system/BoxDrawer.h"

#include "system/imgui/imgui.h"
#include "system/imgui/imgui_impl_dx11.h"
#include "system/imgui/imgui_impl_win32.h"

const uint64_t FIXED_STEP = 16; // 16ms 約60 FPS
static uint64_t g_accumulator = 0; // タイムアキュムレータ

void gameinit()
{
	std::cerr << "[Step 1] Initializing Renderer..." << std::endl;
	Renderer::Init();
	std::cerr << "[Step 1] OK." << std::endl;

	BoxDrawerInit();

	std::cerr << "[Step 2] Initializing DirectInput..." << std::endl;
	CDirectInput::GetInstance().Init(Application::GetHInstance(),
		Application::GetWindow(),
		Application::GetWidth(),
		Application::GetHeight());
	std::cerr << "[Step 2] OK." << std::endl;

	std::cerr << "[Step 3] Initializing DebugUI..." << std::endl;

	DebugUI::Init(Renderer::GetDevice(), Renderer::GetDeviceContext());
	std::cerr << "[Step 3] OK." << std::endl;

	std::cerr << "[Step 4] Initializing SceneManager..." << std::endl;
	SceneManager::GetInstance().Init();
	std::cerr << "[Step 4] OK." << std::endl;

	std::cerr << "[Step 5] Setting Current Scene..." << std::endl;
	
	SceneManager::GetInstance().SetCurrentScene("TitleScene",
		std::make_unique<FadeTransition>(300.0f, FadeTransition::Mode::FadeInOnly));
	std::cerr << "[Step 5] OK." << std::endl;
}

void gameupdate(uint64_t deltatime)
{
	CDirectInput::GetInstance().GetKeyBuffer();		// キーボードの状態を取得
	CDirectInput::GetInstance().GetMouseState();	// マウスの状態を取得

	// シーンマネージャの更新
	SceneManager::GetInstance().Update(FIXED_STEP);

	g_accumulator -= FIXED_STEP;

}

void gamedraw(uint64_t deltatime)
{
	//ImGui フレームを開始
	/*DebugUI::BeginFrame();*/

	// レンダリング前処理
	Renderer::Begin();

	// シーンマネージャの描画
	SceneManager::GetInstance().Draw(deltatime);

	//// デバッグUIの描画
	//DebugUI::Draw();
	////ImGui の描画データを GPU に送信
	//DebugUI::EndFrame();

	// レンダリング後処理
	Renderer::End();
}

void gamedispose()
{
	//// デバッグUIの終了処理
	//DebugUI::DisposeUI();

	// シーンマネージャの終了処理
	SceneManager::GetInstance().Dispose();

	// レンダラの終了処理
	Renderer::Uninit();

}

void gameloop()
{
	uint64_t delta_time = 0;

	// フレームの待ち時間を計算する
	static FPS fpsrate(60);

	// 前回実行されてからの経過時間を計算する
	delta_time = fpsrate.CalcDelta();

	if (delta_time > 100) {
		delta_time = 100;
	}
	//std::cerr << delta_time << std::endl;

	// 更新処理、描画処理を呼び出す
	gameupdate(delta_time);
	gamedraw(delta_time);

	// 規定時間までWAIT
	fpsrate.Tick();

}