#pragma once
#include <vector>
#include <memory>
#include <string>
#include "../system/CSprite.h"

class GameContext;

enum class MenuType 
{
	None,
	Main,//移動、攻撃、終了
	Attack//普通攻撃、プッシュ
};

struct MenuOption 
{
	std::unique_ptr<CSprite> menuSprite;
	Vector3 currentScale{ 1.0f,1.0f,1.0f };
	Vector3 targetScale{ 1.0f,1.0f,1.0f };
	bool isVisible = true;
};

class GameUIManager
{
public:
	void Init(GameContext* context);
	void Update(uint64_t dt);
	void Draw();
	// メニュー状態コントロール
	void OpenMainMenu();
	void OpenAttackMenu();
	void CloseMenu();
	void SetMoveOptionEnabled(bool enabled);
	//メニュー選択確定のアニメーション開始
	void TriggerSelectAnim(int index);//index小さいのは上から
	bool IsAnimating()const { return m_animTimer > 0.0f; }
	//UIリセット
	void Clear();

private:
	GameContext* m_context = nullptr;
	MenuType m_currentType = MenuType::None;

	bool m_isMoveEnabled = true;

	std::vector<MenuOption> m_mainMenuOptions;//0:移動、1:攻撃、2:終了
	std::vector<MenuOption> m_attackMenuOptions;//0:普通攻撃、1:プッシュ

	float m_animTimer = 0.0f;//アニメーションタイマー
	int m_selectedIndex = -1;//選択されたメニューオプションのインデックス
	const float ANIM_DURATION = 0.2f;//アニメーション時間

	void LoadSprite(std::vector<MenuOption>& list, const std::string& path, float w, float h);
	void DrawMenuGroup(std::vector<MenuOption>& list, float startX, float startY);
};