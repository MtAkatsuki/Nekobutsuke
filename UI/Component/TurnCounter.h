#pragma once
#include <vector>
#include <memory>
#include <string>
#include "../../System/CSprite.h"
#include "../../Core/Application.h"

// =========================================================
// TurnCounter クラス
// 残りターン数のUI表示と、ターン更新時のポップアップ演出を管理する
// =========================================================
class TurnCounter {
public:
    enum class AnimState {
        Hidden,       // 非表示状態（敵ターン中など）
        CenterPopUp,  // 画面中央で拡大しながらポップアップ
        CenterWait,   // 中央で一時停止してプレイヤーに注視させる
        MoveToCorner, // 縮小しながら左上（指定座標）へ移動
        Fixed         // 左上の目標地点に固定表示
    };

    TurnCounter() = default;

    void Init() {
        for (int i = 1; i <= MAX_TURN_SPRITES; ++i) {
            std::string path = "Assets/texture/ui/turn_" + std::to_string(i) + ".png";
            auto sprite = std::make_unique<CSprite>(NUM_TEX_W, NUM_TEX_H, path.c_str());
            m_numberSprites.push_back(std::move(sprite));
        }
        m_currentTurn = MAX_TURN_SPRITES;

        m_escapePromptSprite = std::make_unique<CSprite>(PROMPT_TEX_W, PROMPT_TEX_H, "Assets/texture/ui/escape_prompt.png");
        m_state = AnimState::Hidden;
    }

    void SetTurn(int turn) {
        m_currentTurn = turn;
    }

    void StartAnimation() {
        m_animTimer = 0.0f;
        m_state = AnimState::CenterPopUp;

        // 画面中央をスタート位置に設定
        m_currentPos.x = Application::GetWidth() / 2.0f;
        m_currentPos.y = Application::GetHeight() / 2.0f;
        m_currentScale = Vector3(0.0f, 0.0f, 1.0f);
    }

    void Hide() {
        m_state = AnimState::Hidden;
    }

    // ... 前面部分保持不变 ...
    void Update(uint64_t dt) {
        if (m_state == AnimState::Hidden || m_state == AnimState::Fixed) return;

        float deltaSeconds = static_cast<float>(dt) / 1000.0f;
        m_animTimer += deltaSeconds;

        // 目標の固定座標
        Vector3 targetPos = (m_currentTurn <= 0) ? ESCAPE_PROMPT_FIXED_POS : NUMBER_FIXED_POS;

        if (m_state == AnimState::CenterPopUp) {
            float t = m_animTimer / TIME_POPUP;
            if (t >= 1.0f) {
                t = 1.0f;
                m_state = AnimState::CenterWait;
                m_animTimer = 0.0f;
            }
            // 原版の Ease-Out Cubic 緩動
            t = 1.0f - std::pow(1.0f - t, 3.0f);
            float scale = t * 1.0f;
            m_currentScale = Vector3(scale, scale, 1.0f);
        }
        else if (m_state == AnimState::CenterWait) {
            if (m_animTimer >= TIME_WAIT) {
                m_state = AnimState::MoveToCorner;
                m_animTimer = 0.0f;
            }
        }
        else if (m_state == AnimState::MoveToCorner) {
            float t = m_animTimer / TIME_MOVE;
            float easeT = t * t * (3.0f - 2.0f * t);

            if (t >= 1.0f) {
                easeT = 1.0f;
                m_state = AnimState::Fixed;
            }

            Vector3 centerPos(Application::GetWidth() / 2.0f, Application::GetHeight() / 2.0f, 0.0f);
            m_currentPos = Vector3::Lerp(centerPos, targetPos, easeT);

            // 1.0f から 0.8f へ縮小
            float scale = std::lerp(1.0f, FINAL_SCALE, easeT);
            m_currentScale = Vector3(scale, scale, 1.0f);
        }
    }

    void Draw() {
        if (m_state == AnimState::Hidden) return;
		// インデックス０がターン１、インデックス４がターン５に対応
		
		// ターン数が０の場合は脱出提示画像を表示する
        Renderer::SetUISamplerMode(true);

		MATERIAL mtrl;
		mtrl.Diffuse = Color(1.0f, 1.0f, 1.0f, 1.0f);
		mtrl.TextureEnable = TRUE;

		if (m_currentTurn <= 0) {// ターン数が0の場合、脱出提示画像を表示する
			if (m_escapePromptSprite) {
				m_escapePromptSprite->ModifyMtrl(mtrl);
				m_escapePromptSprite->Draw(m_currentScale, Vector3(0, 0, 0), m_currentPos);
			}
		}
		else {// 通常の数字表示
			int index = m_currentTurn - 1;
			if (index >= 0 && index < m_numberSprites.size()) {
				m_numberSprites[index]->ModifyMtrl(mtrl);
				m_numberSprites[index]->Draw(m_currentScale, Vector3(0, 0, 0), m_currentPos);
			}
		}

        Renderer::SetUISamplerMode(false);
    }

    bool IsAnimating() const {
        return m_state == AnimState::CenterPopUp ||
            m_state == AnimState::CenterWait ||
            m_state == AnimState::MoveToCorner;
    }

public:
    // 用意されているターン数字テクスチャの枚数（= 表示可能な最大ターン数）
    // ※ GameScene::INITIAL_TURN_COUNT はこの枚数を超えてはならない
    static constexpr int MAX_TURN_SPRITES = 5;

private:
    // --- 定数 ---
    static constexpr float NUM_TEX_W = 462.0f;
    static constexpr float NUM_TEX_H = 100.0f;
    static constexpr float PROMPT_TEX_W = 502.0f;
    static constexpr float PROMPT_TEX_H = 215.0f;

    static constexpr float TIME_POPUP = 0.3f;
    static constexpr float TIME_WAIT = 1.2f;
    static constexpr float TIME_MOVE = 0.5f;
    static constexpr float FINAL_SCALE = 0.8f;

    // 移動アニメーション終着点（画面左上の固定表示位置）
    static inline const Vector3 ESCAPE_PROMPT_FIXED_POS = Vector3(210.0f, 100.0f, 0.0f);
    static inline const Vector3 NUMBER_FIXED_POS = Vector3(200.0f, 50.0f, 0.0f);

    std::vector<std::unique_ptr<CSprite>> m_numberSprites;
    std::unique_ptr<CSprite> m_escapePromptSprite;

    AnimState m_state = AnimState::Hidden;
    int m_currentTurn = 5;
    float m_animTimer = 0.0f;

    Vector3 m_currentPos;
    Vector3 m_currentScale;
};