#pragma once
#include <memory>
#include <unordered_map>
#include <string>
#include "IScene.h"
#include "noncopyable.h"
#include "SceneTransition.h"

class GameContext;

// =========================================================
// SceneManager クラス (Singleton)
// ゲームシーンのライフサイクル管理と、遷移（トランジション）アニメーションを制御する。
// =========================================================
class SceneManager : NonCopyable {
public:
    static SceneManager& GetInstance() {
        static SceneManager instance;
        return instance;
    }

    // ---------------------------------------------------------
    // ライフサイクル (Lifecycle)
    // ---------------------------------------------------------
    void Init();
    void Dispose();
    void Update(uint64_t deltatime);
    void Draw(uint64_t deltatime);

    // ---------------------------------------------------------
    // シーン遷移制御 (Flow)
    // ---------------------------------------------------------
    // transitionにnullptrを渡した場合は即時切り替えを行う
    void SetCurrentScene(const std::string& sceneName, std::unique_ptr<SceneTransition> transition = nullptr);

    GameContext* GetContext() const { return m_context.get(); }

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

    // --- シーン遷移アニメーション制御 ---
    std::unique_ptr<SceneTransition> m_transition = nullptr;
    std::string m_targetSceneName{};

    // 遷移状態のフラグ（述語）
    bool m_hasSwapped = false;      // 裏側でのシーンインスタンスの切り替えが完了したか
    bool m_isTransitioning = false; // 現在トランジション演出の実行中か
};