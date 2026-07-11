#pragma once
#include <vector>
#include <memory>
#include <string>
#include "../../System/CSprite.h"

class GameContext;

enum class MenuType {
	None,
	Main,   // 移動、攻撃、終了
	Attack  // 普通攻撃、プッシュ
};

struct MenuOption {
	std::unique_ptr<CSprite> menuSprite;
	Vector3 currentScale{ 1.0f, 1.0f, 1.0f };
	Vector3 targetScale{ 1.0f, 1.0f, 1.0f };
	bool isVisible = true;
};

// 矢印UIごとのアニメーション情報を保持する構造体
struct TutorialArrow {
	std::unique_ptr<CSprite> sprite;
	Vector3 baseOffset;   // 基準オフセット
	Vector3 currentShake; // シェイクアニメーションの現在値
	float shakeTimer;     // 個別のタイマー
	bool isVertical;      // 縦揺れか横揺れか
};

// =========================================================
// GameUIManager クラス
// プレイヤーの操作メニュー（移動・攻撃）や、WASD・Enter等の操作ガイドUIを統合管理する
// =========================================================
class GameUIManager {
public:
	GameUIManager() = default;
	~GameUIManager() = default;

	// ---------------------------------------------------------
	// ライフサイクル (Lifecycle)
	// ---------------------------------------------------------
	void Init(GameContext* context);
	void Update(float deltaSeconds);
	void Draw();
	void Clear();

	// ---------------------------------------------------------
	// メニュー制御 (Menu Control)
	// ---------------------------------------------------------
	void OpenMainMenu();
	void CloseMenu();
	void SetMoveOptionEnabled(bool enabled);
	void SetAttackOptionEnabled(bool enabled);

	void TriggerSelectAnim(int index);
	bool IsAnimating() const { return m_animTimer > 0.0f || m_isEventBlocked; }
	void SetEventBlock(bool block) { m_isEventBlocked = block; }

	// ---------------------------------------------------------
	// ガイドUI制御 (Guide UI Control)
	// ---------------------------------------------------------
	// targetPos: アニメーションの中心座標
	void ShowGuideUI(const Vector3& targetPos, bool arrows = true, bool enter = true, bool esc = true);
	void HideGuideUI();
	void SetCameraRotateVisible(bool visible) { m_showCameraRotate = visible; }

	// --- Debug 調節用 ---
	Vector3& GetCameraRotatePos() { return m_cameraRotatePos; }
	float& GetCameraRotateScale() { return m_cameraRotateScale; }

private:
	// ---------------------------------------------------------
	// 内部サブルーチン (Internal Sub-routines)
	// ---------------------------------------------------------
	void LoadSprite(std::vector<MenuOption>& list, const std::string& path, float w, float h);
	void DrawMenuGroup(std::vector<MenuOption>& list, float startX, float startY);
	void UpdateGuideUI(float deltaSeconds);
	void DrawGuideUI();

	// =========================================================
	// メンバー変数
	// =========================================================
	GameContext* m_context = nullptr;

	// --- メニューUI関連 ---
	MenuType m_currentMenu = MenuType::None;
	std::vector<MenuOption> m_mainMenuOptions;

	// --- プレイヤー頭上の矢印用
	float m_arrowTimer = 0.0f;
	float m_arrowHoverY = 0.0f;

	float m_animTimer = 0.0f;
	int m_selectedIndex = -1;
	bool m_isEventBlocked = false;

	// --- ガイドUI関連 ---
	bool m_isGuideActive = false;
	bool m_showArrows = false;
	bool m_showEnter = false;
	bool m_showEsc = false;

	Vector3 m_guideTargetPos{ 0,0,0 };
	float m_guideTimer = 0.0f;

	std::unique_ptr<CSprite> m_escHintSprite;
	std::unique_ptr<CSprite> m_enterHintSprite;
	std::unique_ptr<CSprite> m_activeArrow;
	std::vector<TutorialArrow> m_arrows;


	// --- カメラ回転ガイドUI ---
	std::unique_ptr<CSprite> m_cameraRotateHint;
	bool m_showCameraRotate = false;
	Vector3 m_cameraRotatePos{ 171.0f, 1018.0f, 0.0f };
	float m_cameraRotateScale = 1.0f;
};