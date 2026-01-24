#include "GameUIManager.h"
#include "../manager/GameContext.h"
#include "../gameobject/player.h"
#include "../system/camera.h"
#include "../utility/WorldToScreen.h"
#include "../Application.h"
#include <cmath>

void GameUIManager::Init(GameContext* context) {
    m_context = context;


    LoadSprite(m_mainMenuOptions, "assets/texture/ui/ui_text_move.png", 128, 32);
    LoadSprite(m_mainMenuOptions, "assets/texture/ui/ui_text_attack.png", 128, 32);
    LoadSprite(m_mainMenuOptions, "assets/texture/ui/ui_text_end.png", 128, 32);

    LoadSprite(m_attackMenuOptions, "assets/texture/ui/ui_text_sub_normal.png", 128, 32);
    LoadSprite(m_attackMenuOptions, "assets/texture/ui/ui_text_sub_push.png", 128, 32);
}

void GameUIManager::LoadSprite(std::vector<MenuOption>& list, const std::string& path, float w, float h) {
    MenuOption opt;
    opt.menuSprite = std::make_unique<CSprite>(w, h, path);
	//メニューSpriteを所属のlistに追加
    list.push_back(std::move(opt));
}

void GameUIManager::Update(uint64_t dt) {
    float deltaSeconds = static_cast<float>(dt) / 1000.0f;
	// アニメーション：選択したメニューオプションを拡大、他を消失
    if (m_animTimer > 0.0f) {
        m_animTimer -= deltaSeconds;

		//アップデート用のメニューリストを取得
        std::vector<MenuOption>* targetListPtr = nullptr;
        if (m_currentType == MenuType::Main) targetListPtr = &m_mainMenuOptions;
        else if (m_currentType == MenuType::Attack) targetListPtr = &m_attackMenuOptions;

        if (targetListPtr) {
            std::vector<MenuOption>& currentList = *targetListPtr;
            for (int i = 0; i < currentList.size(); ++i) {
                if (i == 0 && !m_isMoveEnabled) 
                { currentList[i].isVisible = false; continue; }

                if (i == m_selectedIndex) {
                    // 選択したメニューオプションを拡大
                    float t = 1.0f - (m_animTimer / ANIM_DURATION); // 時間比例を計算、0 -> 1
					float scale = 1.0f + sinf(t * 3.14159f) * 0.5f; //拡大率計算
                    currentList[i].currentScale = Vector3(scale, scale, 1.0f);
                    currentList[i].isVisible = true;
                }
                else {
                    // 他を消失
                    currentList[i].isVisible = false;
                }
            }
        }
    }
}
// メニューの描画(プレイヤー位置に基づきて)
void GameUIManager::Draw() {
    if (m_currentType == MenuType::None) return;
    // プレイヤーとカメラを取得
    Player* player = m_context->GetPlayer();
    if (!player) return;
    Camera* camera = m_context->GetCamera();
    // プレイヤーの頭上位置を計算
    Vector3 headPos = player->getSRT().pos;
    headPos.y += 0.1f;

    float screenW = (float)Application::GetWidth();
    float screenH = (float)Application::GetHeight();
    // ワールド座標をスクリーン座標に変換
    Vector2 screenPos = WorldToScreen(headPos, camera->GetViewMatrix(), camera->GetProjMatrix(), screenW, screenH);
    // 画面外なら描画しない
    if (screenPos.x < -50 || screenPos.x > screenW + 50 || screenPos.y < -50 || screenPos.y > screenH + 50) return;
    //プレーヤーの左あたりにメニューを表示
    float startX = screenPos.x - 140.0f;
    float startY = screenPos.y - 60.0f;
	// 現在のメニュータイプに応じてメニューグループを描画
    if (m_currentType == MenuType::Main) {
        DrawMenuGroup(m_mainMenuOptions, startX, startY);
    }
    else if (m_currentType == MenuType::Attack) {
        DrawMenuGroup(m_attackMenuOptions, startX, startY);
    }
}
// メニューグループの描画(メニュー内部の位置)
void GameUIManager::DrawMenuGroup(std::vector<MenuOption>& list, float startX, float startY) {
    float offsetY = 0.0f;
    for (auto& opt : list) {
        if (!opt.isVisible) continue;
		// 各メニューオプションの描画位置を計算
        Vector3 pos(startX, startY + offsetY, 0.0f);
        opt.menuSprite->Draw(opt.currentScale, Vector3(0, 0, 0), pos);

		offsetY += 40.0f; //メニューオプション間の垂直オフセット
    }
}
// メインメニューを開く及び初期化
void GameUIManager::OpenMainMenu() {
    m_currentType = MenuType::Main;
    m_animTimer = 0.0f;
	// メニューオプションをリセット
    for (int i = 0; i < m_mainMenuOptions.size(); ++i) {
        m_mainMenuOptions[i].currentScale = Vector3(1, 1, 1);

		// もし移動メニューオプションなら、移動が有効な場合のみ表示
        if (i == 0) {
            m_mainMenuOptions[i].isVisible = m_isMoveEnabled;
        }
        else {
            m_mainMenuOptions[i].isVisible = true;
        }
    }

	// もし移動メニューが禁止しているなら、最初の選択肢を攻撃に設定
    m_selectedIndex = m_isMoveEnabled ? 0 : 1;
}

// 攻撃メニューを開く及び初期化
void GameUIManager::OpenAttackMenu() {
    m_currentType = MenuType::Attack;
    m_animTimer = 0.0f;
    for (auto& opt : m_attackMenuOptions)
    { opt.isVisible = true; opt.currentScale = Vector3(1, 1, 1); }
}

void GameUIManager::CloseMenu() {
    m_currentType = MenuType::None;
}

void GameUIManager::TriggerSelectAnim(int index) {
    m_animTimer = ANIM_DURATION;
    m_selectedIndex = index;
}

void GameUIManager::SetMoveOptionEnabled(bool enabled) {
    m_isMoveEnabled = enabled;
    // もしメインメニューが開いているなら、移動オプションの表示を更新
    if (!m_mainMenuOptions.empty()) {
        m_mainMenuOptions[0].isVisible = enabled;
    }
}

void GameUIManager::Clear()
{
    // すべてのメニューをリセット
    m_currentType = MenuType::None;
    m_selectedIndex = -1;
    m_animTimer = 0.0f;

    // すべてのメニューを不可視にする
    for (auto& opt : m_mainMenuOptions) {
        opt.isVisible = false;
        opt.currentScale = Vector3(1, 1, 1);
    }
    for (auto& opt : m_attackMenuOptions) {
        opt.isVisible = false;
        opt.currentScale = Vector3(1, 1, 1);
    }
}