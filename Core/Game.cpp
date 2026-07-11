#include "Game.h"
#include "../System/renderer.h"
#include "../System/DebugUI.h"
#include "../System/CDirectInput.h"
#include "../System/scenemanager.h"
#include "../System/Utility/FpsControl.h"
#include "../Core/GameContext.h"
#include "../System/FadeTransition.h"
#include "../System/BoxDrawer.h"
#include "DebugLog.h"

#include <iostream>

namespace {
	// --- 設定・定数  ---
	constexpr uint64_t TARGET_FPS = 60;
	constexpr uint64_t FIXED_STEP_MS = 16;  // 16ms (約60FPS相当の固定ステップ)
	constexpr uint64_t DELTA_TIME_MAX_MS = 100; // delta_timeのクランプ上限（スパイク対策）
	constexpr float    INITIAL_FADE_DURATION = 100.0f;
}

static uint64_t g_accumulator = 0; // 固定ステップ更新用のタイムアキュムレータ

void gameinit()
{
	DBG_TRACE("[Step 1] Initializing Renderer...");
	Renderer::Init();
	DBG_TRACE("[Step 1] OK.");

	BoxDrawerInit();

	DBG_TRACE("[Step 2] Initializing DirectInput...");
	CDirectInput::GetInstance().Init(
		Application::GetHInstance(),
		Application::GetWindow(),
		Application::GetWidth(),
		Application::GetHeight());
	DBG_TRACE("[Step 2] OK.");

	DBG_TRACE("[Step 3] Initializing DebugUI...");

	DebugUI::Init(Renderer::GetDevice(), Renderer::GetDeviceContext());
	DBG_TRACE("[Step 3] OK.");

	DBG_TRACE("[Step 4] Initializing SceneManager...");
	SceneManager::GetInstance().Init();
	DBG_TRACE("[Step 4] OK.");

	DBG_TRACE("[Step 5] Setting Current Scene...");
	
	SceneManager::GetInstance().SetCurrentScene("TitleScene",
		std::make_unique<FadeTransition>(INITIAL_FADE_DURATION, FadeTransition::Mode::FadeInOnly));
	DBG_TRACE("[Step 5] OK.");
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