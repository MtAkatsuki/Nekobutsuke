#include "Game.h"
#include "../System/renderer.h"
#include "../System/DebugUI.h"
#include "../System/CDirectInput.h"
#include "../System/scenemanager.h"
#include "../System/Utility/FpsControl.h"
#include "../Core/GameContext.h"
#include "../System/FadeTransition.h"
#include "../System/BoxDrawer.h"
#include "Application.h"

#include <iostream>

namespace {
	// --- 設定・定数  ---
	constexpr uint64_t TARGET_FPS = 60;
	constexpr uint64_t FIXED_STEP_MS = 16;  // 16ms (約60FPS相当の固定ステップ)
	constexpr uint64_t DELTA_TIME_MAX_MS = 100; // delta_timeのクランプ上限（スパイク対策）
	constexpr float    INITIAL_FADE_DURATION = 200.0f;
}

static uint64_t g_accumulator = 0; // 固定ステップ更新用のタイムアキュムレータ

void gameinit()
{
	std::cerr << "[Step 1] Initializing Renderer..." << std::endl;
	Renderer::Init();
	std::cerr << "[Step 1] OK." << std::endl;

	BoxDrawerInit();

	std::cerr << "[Step 2] Initializing DirectInput..." << std::endl;
	CDirectInput::GetInstance().Init(
		Application::GetHInstance(),
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
		std::make_unique<FadeTransition>(INITIAL_FADE_DURATION, FadeTransition::Mode::FadeInOnly));
	std::cerr << "[Step 5] OK." << std::endl;
}

void gameupdate(uint64_t deltatime){
	// 入力デバイスの更新
	CDirectInput::GetInstance().GetKeyBuffer();
	CDirectInput::GetInstance().GetMouseState();

	// シーンマネージャの更新 (現在は固定ステップで回している)
	SceneManager::GetInstance().Update(FIXED_STEP_MS);

	g_accumulator -= FIXED_STEP_MS;
}

void gamedraw(uint64_t deltatime){
	// F4 キーで ImGui の表示状態を切り替え
	if (CDirectInput::GetInstance().CheckKeyBufferTrigger(DIK_F4)) {
		DebugUI::Toggle();
	}

	if (DebugUI::IsVisible()) {
		DebugUI::BeginFrame();
	}

	// レンダリング前処理
	Renderer::Begin();

	SceneManager::GetInstance().Draw(deltatime);
	Renderer::ResolveToBackbuffer();

	if (DebugUI::IsVisible()) {
		DebugUI::Draw();
		DebugUI::EndFrame(); // ImGui の描画データを GPU に送信
	}

	// --- レンダリングパイプライン終了 ---
	Renderer::Present();
}

void gamedispose(){
	DebugUI::DisposeUI();
	SceneManager::GetInstance().Dispose();
	Renderer::Uninit();

}

void gameloop(){
	static FPS fpsrate(TARGET_FPS);

	// 前回実行されてからの経過時間を計算
	uint64_t delta_time = fpsrate.CalcDelta();

	// スパイク対策：ウィンドウ移動などでOS側で処理が停止した場合、
	// delta_time が異常な値になり物理演算が破綻するのを防ぐ。
	if (delta_time > DELTA_TIME_MAX_MS) {
		delta_time = DELTA_TIME_MAX_MS;
	}

	gameupdate(delta_time);
	gamedraw(delta_time);

	// 次のフレームまで待機(Wait)し、fpsrate内部の時間を更新する
	fpsrate.Tick();
}