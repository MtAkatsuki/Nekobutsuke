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

	// 汎用ガイドUIの表示
	// targetPos: 矢印アニメーションの中心座標（通常はプレイヤーの位置）
	// showArrows: WASD矢印のアニメーションを表示するかどうか
	// showEnter: Enterキーの確認プロンプトを表示するかどうか
	// showEsc: ESCキーのキャンセルプロンプトを表示するかどうか
	void ShowGuideUI(const Vector3& targetPos, bool showArrows, bool showEnter, bool showEsc);

	// ガイドUIの非表示
	void HideGuideUI();

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

	// === ガイドUI関連メンバ ===
	bool m_isGuideActive = false;      // ガイド全体の有効化フラグ（総親機）
	bool m_showArrows = false;         // 矢印を表示するかどうか
	bool m_showEnter = false;          // Enter（決定）を表示するかどうか
	bool m_showEsc = false;            // Esc（キャンセル）を表示するかどうか

	Vector3 m_guideTargetPos{ 0,0,0 };
	float m_guideTimer = 0.0f;

	// ESC キャンセルヒント (右下)
	std::unique_ptr<CSprite> m_escHintSprite;
	// Enter 確認ヒント (右下)
	std::unique_ptr<CSprite> m_enterHintSprite;

	// WASD 方向ヒント (4方向)
	// 1つのスプライトをDraw時に回転・移動させて再利用するか、4つ読み込む
	// 各矢印の揺れ（シェイク）を個別に制御するため、構造体を使用
	struct TutorialArrow {
		std::unique_ptr<CSprite> sprite;
		Vector3 baseOffset;   // 基準オフセット（中心からの相対距離）
		Vector3 currentShake; // 現在のシェイクオフセット
	};
	std::vector<TutorialArrow> m_arrows;

	// ガイドUIの描画
	void DrawGuideUI();
	// ガイドUIの更新
	void UpdateGuideUI(float dt);
};