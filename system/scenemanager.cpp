#include	"scenemanager.h"
#include	"SceneClassFactory.h"
#include	"../manager/GameContext.h"
#include	"../scene/GameScene.h"
#include	"../gameobject/player.h"
#include	"../gameobject/enemy.h"
#include	"../system/DebugUI.h"
#include	<iostream>

SceneManager::SceneManager() = default;

void SceneManager::Init()
{
	m_context = std::make_unique<GameContext>();
	m_context->Init();
}

SceneManager::~SceneManager() { Dispose(); }

// 登録されているシーンを全て破棄する
void SceneManager::Dispose() 
{
	// 登録されているすべてシーンの終了処理
	for (auto& s : m_scenes) 
	{
		s.second->dispose();
	}

	m_scenes.clear();
	m_currentSceneName.clear();
	m_context.reset();
}




void SceneManager::Draw(uint64_t deltatime)
{

	// 現在のシーンを描画
	auto it = m_scenes.find(m_currentSceneName);
	if (it != m_scenes.end() && it->second) {
		it->second->draw(deltatime);
	}

	if (m_transition) 
	{
		m_transition->draw();
	}
}

void SceneManager::Update(uint64_t deltatime)
{
	if (m_transition) 
	{
		m_transition->update(deltatime);
		//FadeOut終了とき、シーン遷移できるか検査する
		if (m_transition->canSwap() && !m_hasSwapped) //遷移状態全部許可得る場合
		{
			InternalChangeScene(m_nextSceneName);//実際遷移実行の場所
			m_hasSwapped = true;
			m_transition->onSceneSwapped();//FadeInを知らせる
		}

		//シーン遷移全部終了か検査
		if (m_transition->isFinished()) 
		{
			m_isTransitioning = false;
			m_transition = nullptr;
		}
	}
	auto it = m_scenes.find(m_currentSceneName);
	// 現在のシーンを更新
	if (it != m_scenes.end() && it->second) {
		it->second->update(deltatime);
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

void SceneManager::SetCurrentScene(const std::string& sceneName,std::unique_ptr<SceneTransition> transition)
{
	DebugUI::ClearDebugFunction();

	if (m_isTransitioning || m_transition != nullptr)
	{
		std::cerr << "[SceneManager] Ignored SetCurrentScene: Transition already active." << std::endl;
		return;
	}
	if (transition)
	{	//もしシーン遷移アニメションがあれば
		m_transition = std::move(transition);
		m_nextSceneName = sceneName;
		m_hasSwapped = false;
		m_isTransitioning = true;

		m_transition->start();
	}
	//アニメションなし、すぐに遷移
	else { InternalChangeScene(sceneName);}
}