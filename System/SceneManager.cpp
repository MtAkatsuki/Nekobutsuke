#include "SceneManager.h"
#include "SceneClassFactory.h"
#include "../Core/GameContext.h"
#include "DebugUI.h"
#include "Audio/AudioManager.h"
#include <iostream>

SceneManager::SceneManager() = default;

SceneManager::~SceneManager() { Dispose(); }

void SceneManager::Init() {
	m_context = std::make_unique<GameContext>();
	m_context->Init();
	AudioManager::GetInstance().Init();
}

void SceneManager::Update(uint64_t deltatime) {
	float deltaSeconds = static_cast<float>(deltatime) / 1000.0f;

	// 常駐システムの更新
	AudioManager::GetInstance().Update(deltaSeconds);

	// --- トランジション（画面遷移）の進行管理 ---
	if (m_transition) {
		m_transition->Update(deltatime);

		// 画面が完全に暗転（または覆われた）タイミングで実際のシーンを切り替える
		if (m_transition->canSwap() && !m_hasSwapped) {
			InternalChangeScene(m_targetSceneName);
			m_hasSwapped = true;
			m_transition->OnSceneSwapped(); // 新シーン描画開始（フェードイン等へ移行）
		}

		// 遷移演出がすべて終了した際のクリーンアップ
		if (m_transition->isFinished()) {
			m_isTransitioning = false;
			m_transition = nullptr;
		}
	}

	// --- 現在のシーンの更新 ---
	auto it = m_scenes.find(m_currentSceneName);
	if (it != m_scenes.end() && it->second) {
		it->second->Update(deltatime);
	}
}

void SceneManager::Draw(uint64_t deltatime) {
	// 現在のシーンを描画
	auto it = m_scenes.find(m_currentSceneName);
	if (it != m_scenes.end() && it->second) {
		it->second->Draw(deltatime);
	}
	// シーンの上にトランジション（暗転用ポリゴンなど）を被せて描画
	if (m_transition)
	{
		m_transition->Draw();
	}
}

void SceneManager::Dispose() {
	for (auto& s : m_scenes) {
		if (s.second) {
			s.second->Dispose();
		}
	}
	m_scenes.clear();
	m_currentSceneName.clear();
	m_context.reset();
}

void SceneManager::SetCurrentScene(const std::string& sceneName, std::unique_ptr<SceneTransition> transition) {
	DebugUI::ClearDebugFunction(); // 遷移時に不要なデバッグUIを破棄

	// 二重遷移の防止（防御的プログラミング）
	if (m_isTransitioning || m_transition != nullptr) {
		std::cerr << "[SceneManager] Warning: Ignored SetCurrentScene. Transition already active." << std::endl;
		return;
	}

	if (transition) {
		m_transition = std::move(transition);
		m_targetSceneName = sceneName;
		m_hasSwapped = false;
		m_isTransitioning = true;

		m_transition->Start();
	}
	else {
		// トランジションなしの場合は即時切り替え
		InternalChangeScene(sceneName);
	}
}

void SceneManager::InternalChangeScene(const std::string& sceneName) 
{
	std::cerr << "[SceneManager] Swapping to" << sceneName << std::endl;
	m_currentSceneName = sceneName;

	auto nowScene = SceneClassFactory::getInstance().create(sceneName);
	if (nowScene) 
	{
		m_scenes.clear();//現在使用中のシーンを削除
		nowScene->SetGameContext(m_context.get());
		nowScene->Init();
		m_scenes[m_currentSceneName] = std::move(nowScene);
	}
	else { std::cerr << "[Error] Scene Not Found: " << sceneName << std::endl; }
}
