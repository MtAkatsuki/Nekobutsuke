#pragma once
#include <memory>
#include <unordered_map>
#include <string>
#include <array>
#include "IScene.h"
#include "noncopyable.h"
#include "../system/SceneTransition.h"

// 前方宣言
class GameContext;


class SceneManager : NonCopyable{



public:
	static SceneManager& GetInstance() {
		static SceneManager instance;
		return instance;
	}
	//transition=nullptrは許す、nullptrなたら、すぐに切り替える
	void SetCurrentScene(const std::string& sceneName, std::unique_ptr<SceneTransition> transition = nullptr);
	void Dispose();
	void Init();
	void Update(uint64_t deltatime);
	void Draw(uint64_t deltatime);
	GameContext* GetContext() { return m_context.get(); }

private:
	SceneManager();
	~SceneManager();
	//コンストラクタのコピーは禁止、SceneManager scene2(scene1)ようなやり方は禁止されている
	SceneManager(const SceneManager&) = delete;
	//scene1 = scene2みたいなやり方で、SceneManager オブジェクト同士の代入を禁止する
	SceneManager& operator=(const SceneManager&) = delete;

	void InternalChangeScene(const std::string& sceneName);

private:
	std::unique_ptr<GameContext> m_context{ nullptr };
	std::unordered_map<std::string, std::unique_ptr<IScene>> m_scenes{};
	std::string m_currentSceneName{};

	//シーン遷移アニメション変数
	std::unique_ptr<SceneTransition> m_transition = nullptr;
	std::string m_nextSceneName;//次のシーン
	bool m_hasSwapped = false;//切り替える完成のマーク
	bool m_isTransitioning = false;//切り替え中
};
