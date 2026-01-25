#include "GameUIManager.h"
#include "../manager/GameContext.h"
#include "../gameobject/player.h"
#include "../system/camera.h"
#include "../utility/WorldToScreen.h"
#include "../Application.h"
#include <cmath>
#include <iostream>

void GameUIManager::Init(GameContext* context) {
    m_context = context;


    LoadSprite(m_mainMenuOptions, "assets/texture/ui/ui_text_move.png", 128, 32);
    LoadSprite(m_mainMenuOptions, "assets/texture/ui/ui_text_attack.png", 128, 32);
    LoadSprite(m_mainMenuOptions, "assets/texture/ui/ui_text_end.png", 128, 32);

    LoadSprite(m_attackMenuOptions, "assets/texture/ui/ui_text_sub_normal.png", 128, 32);
    LoadSprite(m_attackMenuOptions, "assets/texture/ui/ui_text_sub_push.png", 128, 32);

    m_escHintSprite = std::make_unique<CSprite>(197, 43.5, "assets/texture/ui/ui_text_cancel.png");
    m_enterHintSprite = std::make_unique<CSprite>(197, 42.5, "assets/texture/ui/ui_text_enter.png");

    // 2. WASD 矢印 (4つの独立した画像)
    // 順番：W(上), S(下), A(左), D(右)
    std::string arrowFiles[4] = {
        "assets/texture/ui/ui_arrow_w.png",
        "assets/texture/ui/ui_arrow_s.png",
        "assets/texture/ui/ui_arrow_a.png",
        "assets/texture/ui/ui_arrow_d.png"
    };

    // 既存データをクリア
    m_arrows.clear();

    for (int i = 0; i < 4; ++i) {
        //制御用構造体
        TutorialArrow arrow;
        
        arrow.sprite = std::make_unique<CSprite>(87.5f, 59, arrowFiles[i]);
        arrow.currentShake = Vector3(0, 0, 0);

        // 4方向のオフセットを設定 
        float offsetDist = 90.0f;

        if (i == 0) { // W (上)
            arrow.baseOffset = Vector3(0, -offsetDist, 0);
        }
        else if (i == 1) { // S (下)
            arrow.baseOffset = Vector3(0, offsetDist, 0);
        }
        else if (i == 2) { // A (左)
            arrow.baseOffset = Vector3(-offsetDist, 0, 0);
        }
        else if (i == 3) { // D (右)
            arrow.baseOffset = Vector3(offsetDist, 0, 0);
        }
        m_arrows.push_back(std::move(arrow));
    }

    m_isGuideActive = false;
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
	// ガイドUIの更新
    if (m_isGuideActive) {
        UpdateGuideUI(deltaSeconds);
    }
}
// メニューの描画(プレイヤー位置に基づきて)
void GameUIManager::Draw() {
	// ガイドUIの描画
    if (m_isGuideActive) {
        DrawGuideUI();
    }
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


//===ガイドUIの実装 ===
// ガイドUIの開始(プレイヤー位置を引数に取る)
void GameUIManager::ShowGuideUI(const Vector3& targetPos, bool showArrows, bool showEnter, bool showEsc) {
    m_isGuideActive = true;
    m_guideTargetPos = targetPos;

    m_showArrows = showArrows;
    m_showEnter = showEnter;
    m_showEsc = showEsc;
    // タイマーを必ずリセットする
    // これがないと、2回目以降に表示する際、前回のタイマー値が残ってしまい、いきなり終了判定(非表示)になる
    m_guideTimer = 0.0f;

    // 矢印を表示する必要がある場合のみタイマーをリセットし、テキストのみの表示時に不要なリセット処理が走るのを防ぐ
    // 実装の簡略化と堅牢性を考慮し、呼び出しのたびに状態を初期化している
    for (auto& arrow : m_arrows) arrow.currentShake = Vector3(0, 0, 0);
}

void GameUIManager::HideGuideUI() {
    m_isGuideActive = false;
    m_showArrows = false;
    m_showEnter = false;
    m_showEsc = false;
}

void GameUIManager::UpdateGuideUI(float dt) {
    if (!m_showArrows) return; // 矢印を表示しない場合は、アニメーションロジックを実行する必要はない

    m_guideTimer += dt;

    // アニメーションフェーズの設定
    const float TIME_POP_IN = 0.2f;   // ポップイン（出現）時間
    const float TIME_SHAKE = 0.6f;   // シェイク（揺れ）時間

    // シェイク処理（拡大演出の完了後に開始）
    if (m_guideTimer > TIME_POP_IN && m_guideTimer < (TIME_POP_IN + TIME_SHAKE)) {
        float shakeStrength = 5.0f; // 揺れの強さ
        float shakeSpeed = 20.0f;  // 揺れの速さ
        // シェイク開始からの経過時間を計算
        // TIME_POP_IN はシェイクが開始されるタイミングのタイムスタンプ
        // elapsedShakeTime = 0 の瞬間にシェイクが開始される
        // 時間の経過とともに elapsedShakeTime の値は増加する
        float elapsedShakeTime = m_guideTimer - TIME_POP_IN;
        float offset = sinf(elapsedShakeTime * shakeSpeed) * shakeStrength;

        // 各方向に異なる揺れを適用し、キー入力のような演出を行う
        // W/S は上下、A/D は左右に揺らす
        if (m_arrows.size() >= 4) {
            m_arrows[0].currentShake = Vector3(0, offset, 0);  // W
            m_arrows[1].currentShake = Vector3(0, -offset, 0); // S
            m_arrows[2].currentShake = Vector3(offset, 0, 0);  // A
            m_arrows[3].currentShake = Vector3(-offset, 0, 0); // D
        }
    }
    else {
        // 期間外は揺れをリセット
        for (auto& arrow : m_arrows) arrow.currentShake = Vector3(0, 0, 0);
    }
}

void GameUIManager::DrawGuideUI() {

    float screenW = (float)Application::GetWidth();
    float screenH = (float)Application::GetHeight();

    // 1. ESC ヒントの描画 (右下)
    // レイアウト：ボトムマージン 20
    float bottomMargin = 20.0f;
    float spriteHeight = 32.0f;
    float spriteWidth = 128.0f;
    float rightMargin = 20.0f;

    float escX = screenW - spriteWidth - rightMargin;
    float escY = screenH - spriteHeight - bottomMargin;

    if (m_showEsc && m_escHintSprite) {
        m_escHintSprite->Draw(Vector3(1, 1, 1), Vector3(0, 0, 0), Vector3(escX, escY, 0));
    }

    // 2. ENTER ヒントの描画 (ESC の上)
    // レイアウト：ESC の上部に配置、間隔は 10

    if (m_showEnter && m_enterHintSprite) {
        float gap = 20.0f;
        float enterY = escY - spriteHeight - gap;
        m_enterHintSprite->Draw(Vector3(1, 1, 1), Vector3(0, 0, 0), Vector3(escX, enterY, 0));
    }
    // 3. WASD 矢印の描画（アニメーション付き）
    if (m_showArrows && m_guideTimer < 1.0f) {

        Camera* camera = m_context->GetCamera();
        if (!camera) return;

        // プレイヤーの頭上に表示するための座標計算
        Vector3 targetPos = m_guideTargetPos;
        targetPos.y += 0.5f;
        Vector2 screenPos = WorldToScreen(targetPos, camera->GetViewMatrix(), camera->GetProjMatrix(), screenW, screenH);

        // 画面外判定
        if (screenPos.x < -100 || screenPos.x > screenW + 100 || screenPos.y < -100 || screenPos.y > screenH + 100) return;

        // スケール計算（イージング演出）
        float scale = 0.0f;
        const float TIME_POP_IN = 0.2f;
        const float TIME_FADE_START = 0.8f;
        const float TIME_END = 1.0f;

        if (m_guideTimer < TIME_POP_IN) {
            // ポップイン：少し弾むような動き（簡略化したイージング）
            // 時間を [0, 1] の範囲に正規化
            scale = m_guideTimer / TIME_POP_IN;
            //scale = 1 + 2x^3 + x^2
            scale = 1.0f + 2.0f * pow(scale - 1.0f, 3.0f) + pow(scale - 1.0f, 2.0f);
            if (scale > 1.0f) scale = 1.0f;
        }
        else if (m_guideTimer > TIME_FADE_START) {
            // フェードアウト
            //フェードアウトからの経過時間
            float elapsed = m_guideTimer - TIME_FADE_START;// elapsed ∈ [0, (TIME_END - TIME_FADE_START)]
            //フェードアウト アニメション全体の期間
            float duration = TIME_END - TIME_FADE_START;
            // t ∈ [0, 1]
            // t=0: フェードアウト開始
            // t=1: フェードアウト終わり
            float t = elapsed / duration;
            scale = 1.0f - t;
        }
        else {
            scale = 1.0f;
        }
        if (scale < 0.0f) scale = 0.0f;

        // 4つの矢印を描画
        for (const auto& arrow : m_arrows) {
            if (!arrow.sprite) continue;

            Vector3 drawPos;
            drawPos.x = screenPos.x + arrow.baseOffset.x + arrow.currentShake.x;
            drawPos.y = screenPos.y + arrow.baseOffset.y + arrow.currentShake.y;
            drawPos.z = 0.0f;

            // 回転なしで描画
            arrow.sprite->Draw(Vector3(scale, scale, 1.0f), Vector3(0, 0, 0), drawPos);
        }
    }
}