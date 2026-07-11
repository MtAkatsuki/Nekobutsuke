#include "GameUIManager.h"
#include "../../Core/GameContext.h"
#include "../../Actor/Character/Player.h"
#include "../../System/Camera.h"
#include "../../System/Utility/WorldToScreen.h"
#include "../../Core/Application.h"
#include <cmath>

namespace {
    // --- メニュー設定 ---
    const float MENU_OFFSET_X = 140.0f;      // プレイヤー頭上基準のメニュー左オフセット
    const float MENU_OFFSET_Y = 60.0f;       // プレイヤー頭上基準のメニュー上オフセット
    const float MENU_ITEM_SPACING_Y = 45.0f; // メニュー項目間の縦間隔
    const float MENU_ANIM_DURATION = 0.2f;
    const float MENU_SELECT_POP_SCALE = 0.5f;   // 選択アニメーションの拡大量（sinカーブ振幅）
    const float MENU_ANCHOR_Y_OFFSET = 0.1f;    // メニュー基準点のワールドY補正

    // --- プレイヤー頭上矢印 ---
    const float ACTIVE_ARROW_Y_OFFSET = 1.5f;   // 矢印のワールド空間での頭上高さ
    const float ARROW_HOVER_SPEED = 3.0f;       // 矢印の上下ホバー速度
    const float ARROW_HOVER_AMP = 10.0f;        // 矢印の上下ホバー振幅（px）

    // --- 画面外カリング ---
    const float SCREEN_CULL_MARGIN = 50.0f;     // この余白を超えて画面外なら描画しない

    // --- UIスプライトのテクスチャ実寸（px） ---
    const float MENU_TEX_W = 168.0f;
    const float MENU_TEX_H = 42.0f;
    const float ESC_HINT_TEX_W = 197.0f;
    const float ESC_HINT_TEX_H = 43.5f;
    const float ENTER_HINT_TEX_W = 197.0f;
    const float ENTER_HINT_TEX_H = 42.5f;
    const float GUIDE_ARROW_TEX_W = 87.5f;
    const float GUIDE_ARROW_TEX_H = 59.0f;
    const float ACTIVE_ARROW_TEX_W = 53.0f;
    const float ACTIVE_ARROW_TEX_H = 66.0f;

    // --- カメラ回転ヒントUI ---
    const float CAM_HINT_SCALE = 0.5f;          // 表示スケール
    const float CAM_HINT_POS_X = 87.0f;         // 画面左下の配置X
    const float CAM_HINT_TEX_W = 306.0f;        // テクスチャ実寸幅（px）
    const float CAM_HINT_TEX_H = 270.0f;        // テクスチャ実寸高さ（px）
    const float CAM_HINT_Y_ADJUST = 50.0f;      // 配置Yの微調整（下方向へずらす量）

    // --- ガイドUIアニメーション設定 ---
    const float GUIDE_SHAKE_SPEED = 20.0f;   // 矢印が揺れる速度
    const float GUIDE_SHAKE_AMP = 5.0f;      // 矢印が揺れる振幅(ピクセル)

    const float GUIDE_TARGET_Y_OFFSET = 0.5f;// ガイド矢印の基準点を頭上へ持ち上げる量
    const float GUIDE_TIME_POP_IN = 0.2f;    // 出現にかかる時間
    const float GUIDE_TIME_FADE_START = 0.8f;// 消滅を開始するまでの時間
    const float GUIDE_TIME_END = 1.0f;       // 完全に消滅する時間

    // 右下UIの配置オフセット
    const float ESC_UI_OFFSET_X = -148.0f;
    const float ESC_UI_OFFSET_Y = -52.0f;
    const float ENTER_UI_OFFSET_X = -148.0f;
    const float ENTER_UI_OFFSET_Y = -104.0f;
}

void GameUIManager::Init(GameContext* context) {
    m_context = context;

    // 1. メインメニューのロード
    LoadSprite(m_mainMenuOptions, "Assets/texture/ui/ui_text_move.png", MENU_TEX_W, MENU_TEX_H);
    LoadSprite(m_mainMenuOptions, "Assets/texture/ui/ui_text_attack.png", MENU_TEX_W, MENU_TEX_H);
    LoadSprite(m_mainMenuOptions, "Assets/texture/ui/ui_text_end.png", MENU_TEX_W, MENU_TEX_H);

    m_escHintSprite = std::make_unique<CSprite>(ESC_HINT_TEX_W, ESC_HINT_TEX_H, "Assets/texture/ui/ui_text_cancel.png");
    m_enterHintSprite = std::make_unique<CSprite>(ENTER_HINT_TEX_W, ENTER_HINT_TEX_H, "Assets/texture/ui/ui_text_enter.png");

    // 2. WASD 矢印のロード
    std::string arrowFiles[4] = {
        "Assets/texture/ui/ui_arrow_w.png",
        "Assets/texture/ui/ui_arrow_s.png",
        "Assets/texture/ui/ui_arrow_a.png",
        "Assets/texture/ui/ui_arrow_d.png"
    };

    m_arrows.clear();
    float offsetDist = 90.0f; // 厳密なオリジナルパラメータ

    for (int i = 0; i < 4; ++i) {
        TutorialArrow arrow;
        arrow.sprite = std::make_unique<CSprite>(GUIDE_ARROW_TEX_W, GUIDE_ARROW_TEX_H, arrowFiles[i]); // オリジナルサイズ
        arrow.currentShake = Vector3(0, 0, 0);

        // オリジナルのオフセット設定
        if (i == 0)      arrow.baseOffset = Vector3(0, -offsetDist, 0); // W (上)
        else if (i == 1) arrow.baseOffset = Vector3(0, offsetDist, 0);  // S (下)
        else if (i == 2) arrow.baseOffset = Vector3(-offsetDist, 0, 0); // A (左)
        else if (i == 3) arrow.baseOffset = Vector3(offsetDist, 0, 0);  // D (右)

        m_arrows.push_back(std::move(arrow));
    }

    m_isGuideActive = false;

    // 3. プレイヤー頭上の指示矢印を復元
    m_activeArrow = std::make_unique<CSprite>(ACTIVE_ARROW_TEX_W, ACTIVE_ARROW_TEX_H, "Assets/texture/ui/ui_arrow_down.png");

    // 4. カメラ回転UIをロードし、初期位置を設定
    m_cameraRotateScale = CAM_HINT_SCALE;
    m_cameraRotateHint = std::make_unique<CSprite>(CAM_HINT_TEX_W, CAM_HINT_TEX_H, "Assets/texture/ui/ui_text_camera_rotate.png");

    m_cameraRotatePos.x = CAM_HINT_POS_X;
    m_cameraRotatePos.y = (float)Application::GetHeight() - (CAM_HINT_TEX_H * m_cameraRotateScale) + CAM_HINT_Y_ADJUST;
}

void GameUIManager::Update(uint64_t dt) {
    float deltaSeconds = static_cast<float>(dt) / 1000.0f;

    // メニュー選択アニメーションの進行
    if (m_animTimer > 0.0f) {
        m_animTimer -= deltaSeconds;
        if (m_animTimer <= 0.0f) m_animTimer = 0.0f;

        for (int i = 0; i < m_mainMenuOptions.size(); ++i) {
            if (i == m_selectedIndex) {
                float t = 1.0f - (m_animTimer / MENU_ANIM_DURATION);
                float scale = 1.0f + std::sinf(t * PI) * MENU_SELECT_POP_SCALE; // ポップアップ脈動効果
                m_mainMenuOptions[i].currentScale = Vector3(scale, scale, 1.0f);
                m_mainMenuOptions[i].isVisible = true;
            }
            else {
                m_mainMenuOptions[i].isVisible = false;
            }
        }
    }

    UpdateGuideUI(deltaSeconds);

    // プレイヤー頭上の矢印用単振動
    if (m_context && m_context->GetTurnManager() && m_context->GetTurnManager()->GetTurnState() == TurnState::PlayerPhase) {
        m_arrowTimer += deltaSeconds;
        m_arrowHoverY = std::sinf(m_arrowTimer * ARROW_HOVER_SPEED) * ARROW_HOVER_AMP;
    }
}

void GameUIManager::Draw() {
    Renderer::SetUISamplerMode(true);

    Player* player = m_context->GetPlayer();
    Camera* camera = m_context->GetCamera();

    if (player && camera) {
        float screenW = (float)Application::GetWidth();
        float screenH = (float)Application::GetHeight();

        // 1. プレイヤー頭上の矢印描画
        if (m_context->GetTurnManager()->GetTurnState() == TurnState::PlayerPhase) {
            Vector3 arrowWorldPos = player->GetSRT().pos;
            arrowWorldPos.y += ACTIVE_ARROW_Y_OFFSET;

            Vector2 arrowScreenPos = WorldToScreen(arrowWorldPos, camera->GetViewMatrix(), camera->GetProjMatrix(), screenW, screenH);

            if (arrowScreenPos.x > -SCREEN_CULL_MARGIN && arrowScreenPos.x < screenW + SCREEN_CULL_MARGIN &&
                arrowScreenPos.y > -SCREEN_CULL_MARGIN && arrowScreenPos.y < screenH + SCREEN_CULL_MARGIN) {

                Vector3 arrowPos(std::round(arrowScreenPos.x), std::round(arrowScreenPos.y + m_arrowHoverY), 0);
                if (m_activeArrow) {
                    m_activeArrow->Draw(Vector3(1.0f, 1.0f, 1.0f), Vector3(0, 0, 0), arrowPos);
                }
            }
        }

        // 2. メニューUI描画
        if (m_currentMenu == MenuType::Main) {
            Vector3 headPos = player->GetSRT().pos;
            headPos.y += MENU_ANCHOR_Y_OFFSET;
            Vector2 screenPos = WorldToScreen(headPos, camera->GetViewMatrix(), camera->GetProjMatrix(), screenW, screenH);

            if (!(screenPos.x < -SCREEN_CULL_MARGIN || screenPos.x > screenW + SCREEN_CULL_MARGIN ||
                  screenPos.y < -SCREEN_CULL_MARGIN || screenPos.y > screenH + SCREEN_CULL_MARGIN)) {
                float startX = screenPos.x - MENU_OFFSET_X;
                float startY = screenPos.y - MENU_OFFSET_Y;
                DrawMenuGroup(m_mainMenuOptions, startX, startY);
            }
        }
    }

    if (m_showCameraRotate && m_cameraRotateHint) {
        m_cameraRotateHint->Draw(
            Vector3(m_cameraRotateScale, m_cameraRotateScale, 1.0f),
            Vector3(0, 0, 0),
            m_cameraRotatePos
        );
    }

    DrawGuideUI();
    Renderer::SetUISamplerMode(false);
}

void GameUIManager::Clear() {
    CloseMenu();
    HideGuideUI();
}

void GameUIManager::OpenMainMenu() {
    if (m_currentMenu == MenuType::Main) return;
    m_currentMenu = MenuType::Main;

    for (int i = 0; i < m_mainMenuOptions.size(); ++i) {
        m_mainMenuOptions[i].currentScale = Vector3(1.0f, 1.0f, 1.0f);
        m_mainMenuOptions[i].targetScale = Vector3(1.0f, 1.0f, 1.0f);
		// デフォルトでは「移動」と「攻撃」を有効、「終了」は非表示
        if (i == 2) {
            m_mainMenuOptions[i].isVisible = true;
        }
    }
}

void GameUIManager::CloseMenu() {
    m_currentMenu = MenuType::None;
}

void GameUIManager::SetMoveOptionEnabled(bool enabled) {
    if (m_mainMenuOptions.size() > 0) m_mainMenuOptions[0].isVisible = enabled;
}

void GameUIManager::SetAttackOptionEnabled(bool enabled) {
    if (m_mainMenuOptions.size() > 1) m_mainMenuOptions[1].isVisible = enabled;
}

void GameUIManager::TriggerSelectAnim(int index) {
    if (m_currentMenu == MenuType::Main && index >= 0 && index < m_mainMenuOptions.size()) {
        m_selectedIndex = index;
        m_mainMenuOptions[index].targetScale = Vector3(1.5f, 1.5f, 1.0f);
        m_animTimer = MENU_ANIM_DURATION;
    }
}

void GameUIManager::ShowGuideUI(const Vector3& targetPos, bool arrows, bool enter, bool esc) {
    m_isGuideActive = true;
    m_guideTargetPos = targetPos;
    m_showArrows = arrows;
    m_showEnter = enter;
    m_showEsc = esc;
    m_guideTimer = 0.0f;
}

void GameUIManager::HideGuideUI() {
    m_isGuideActive = false;
}

void GameUIManager::LoadSprite(std::vector<MenuOption>& list, const std::string& path, float w, float h) {
    MenuOption opt;
    opt.menuSprite = std::make_unique<CSprite>(w, h, path);
    list.push_back(std::move(opt));
}

void GameUIManager::DrawMenuGroup(std::vector<MenuOption>& list, float startX, float startY) {
    float offsetY = 0.0f;
    for (auto& opt : list) {
        if (!opt.isVisible) continue;
		// 各メニューオプションの描画位置を計算
        Vector3 pos(startX, startY + offsetY, 0.0f);
        opt.menuSprite->Draw(opt.currentScale, Vector3(0, 0, 0), pos);

		offsetY += MENU_ITEM_SPACING_Y; //メニューオプション間の垂直オフセット
    }
}

void GameUIManager::UpdateGuideUI(float deltaSeconds) {
    if (!m_isGuideActive) return;

    m_guideTimer += deltaSeconds;

    // 自動フェードアウト時間を超えたら非表示にする
    if (m_guideTimer > GUIDE_TIME_END) {
        m_isGuideActive = false;
        return;
    }

    // 各矢印のシェイク（揺れ）位置を計算する
    for (auto& arrow : m_arrows) {
        arrow.shakeTimer += deltaSeconds;
        float offsetVal = std::sinf(arrow.shakeTimer * GUIDE_SHAKE_SPEED) * GUIDE_SHAKE_AMP;
        if (arrow.isVertical) {
            arrow.currentShake = Vector3(0.0f, offsetVal, 0.0f);
        }
        else {
            arrow.currentShake = Vector3(offsetVal, 0.0f, 0.0f);
        }
    }
}

void GameUIManager::DrawGuideUI() {
    if (!m_isGuideActive || !m_context || !m_context->GetCamera()) return;

    float sw = static_cast<float>(Application::GetWidth());
    float sh = static_cast<float>(Application::GetHeight());
    Camera* cam = m_context->GetCamera();

    Vector2 screenPos = WorldToScreen(m_guideTargetPos, cam->GetViewMatrix(), cam->GetProjMatrix(), sw, sh);

    // 経過時間に基づくアルファ(透明度)とスケールの計算
    float scale = 1.0f;
    if (m_guideTimer < GUIDE_TIME_POP_IN) {
        // 弾むようなポップイン（Overshoot）イージング
        scale = m_guideTimer / GUIDE_TIME_POP_IN;
        scale = 1.0f + 2.0f * std::pow(scale - 1.0f, 3.0f) + std::pow(scale - 1.0f, 2.0f);
    }
    else if (m_guideTimer > GUIDE_TIME_FADE_START) {
        // フェードアウト
        float elapsed = m_guideTimer - GUIDE_TIME_FADE_START;
        float duration = GUIDE_TIME_END - GUIDE_TIME_FADE_START;
        float t = elapsed / duration;
        scale = std::max(0.0f, 1.0f - t);
    }

    MATERIAL mtrl;
    mtrl.Diffuse = Color(1.0f, 1.0f, 1.0f, std::min(scale, 1.0f));
    mtrl.TextureEnable = TRUE;

    if (m_showArrows) {
        Vector3 targetPos = m_guideTargetPos;
        targetPos.y += GUIDE_TARGET_Y_OFFSET;

        Vector2 screenPos = WorldToScreen(targetPos, cam->GetViewMatrix(), cam->GetProjMatrix(), sw, sh);

        for (const auto& arrow : m_arrows) {
            if (!arrow.sprite) continue;
            Vector3 drawPos;
            drawPos.x = screenPos.x + arrow.baseOffset.x + arrow.currentShake.x;
            drawPos.y = screenPos.y + arrow.baseOffset.y + arrow.currentShake.y;
            drawPos.z = 0.0f;

            arrow.sprite->ModifyMtrl(mtrl);
            arrow.sprite->Draw(Vector3(scale, scale, 1.0f), Vector3(0, 0, 0), drawPos);
        }
    }

    if (m_showEsc && m_escHintSprite) {
        Vector3 pos(sw + ESC_UI_OFFSET_X, sh + ESC_UI_OFFSET_Y, 0.0f);
        m_escHintSprite->ModifyMtrl(mtrl);
        m_escHintSprite->Draw(Vector3(scale, scale, 1.0f), Vector3(0, 0, 0), pos);
    }

    if (m_showEnter && m_enterHintSprite) {
        Vector3 pos(sw + ENTER_UI_OFFSET_X, sh + ENTER_UI_OFFSET_Y, 0.0f);
        m_enterHintSprite->ModifyMtrl(mtrl);
        m_enterHintSprite->Draw(Vector3(scale, scale, 1.0f), Vector3(0, 0, 0), pos);
    }
}