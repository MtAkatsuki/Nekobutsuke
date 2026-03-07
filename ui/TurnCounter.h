#pragma once
#include <vector>
#include <memory>
#include <string>
#include "../system/CSprite.h"
#include "../Application.h"

class TurnCounter {
public:

	// --- ダイアログやUI演出の状態を管理する列挙型 ---
	enum class AnimState {
		Hidden,       // 非表示状態（敵ターン中など）
		CenterPopUp,  // 画面中央で拡大しながらポップアップ
		CenterWait,   // 中央で一時停止してプレイヤーに注視させる
		MoveToCorner, // 縮小しながら左上（指定座標）へ移動
		Fixed         // 左上の目標地点に固定表示
	};

    TurnCounter() {};
    ~TurnCounter() {};

    void Init() {
		// ターンカウンター用のスプライトを１～５まで読み込む
        for (int i = 1; i <= 5; ++i) {
            std::string path = "assets/texture/ui/turn_" + std::to_string(i) + ".png";
            auto sprite = std::make_unique<CSprite>(462, 100, path.c_str());
            m_numberSprites.push_back(std::move(sprite));
        }
        m_currentTurn = 5;

        m_escapePromptSprite = std::make_unique<CSprite>(502, 215, "assets/texture/ui/escape_prompt.png");
		//最初は不表示
		m_state = AnimState::Hidden;
    }

    void SetTurn(int turn) {
		// 範囲は１～５に制限
        if (turn < 0) turn = 0;
        if (turn > 5) turn = 5;
        m_currentTurn = turn;
    }

	// 飛行アニメーションの開始（中央から隅への移動シーケンス）
	void StartAnimation() {
		m_state = AnimState::CenterPopUp;
		m_timer = 0.0f;
		m_currentScale = Vector3(0.0f, 0.0f, 1.0f);

		float screenW = (float)Application::GetWidth();
		float screenH = (float)Application::GetHeight();
		m_currentPos = Vector3(screenW / 2.0f, screenH / 2.0f, 0.0f);
	}

	// 隠す、次のターン始まる前に
	void Hide() {
		m_state = AnimState::Hidden;
	}

	// 入場もしくは飛行アニメションしているか
	bool IsAnimating() const {
		return m_state == AnimState::CenterPopUp ||
			m_state == AnimState::CenterWait ||
			m_state == AnimState::MoveToCorner;
	}

    void Update(uint64_t dt) {
        //もし不表示、固定中なら、RETURN
        if (m_state == AnimState::Hidden || m_state == AnimState::Fixed) return;

        float deltaSeconds = static_cast<float>(dt) / 1000.0f;

        if (deltaSeconds > 0.1f) deltaSeconds = 0.016f;

        m_timer += deltaSeconds;

        float screenW = (float)Application::GetWidth();
        float screenH = (float)Application::GetHeight();
        Vector3 centerPos(screenW / 2.0f, screenH / 2.0f, 0.0f);

        // 画像の種類（ターン数か脱出指示か）に応じて目標座標を切り替え
        Vector3 targetPos = (m_currentTurn <= 0) ? Vector3(210.0f, 100.0f, 0.0f) : Vector3(200.0f, 50.0f, 0.0f);

        // アニメーションタイムライン設定
        const float TIME_POPUP = 0.3f; // ポップアップ時間
        const float TIME_WAIT = 1.5f;  // 中央待機終了時間(0.3 ~ 1.5)
        const float TIME_MOVE = 2.0f;  // 移動完了時間(1.5 ~ 2.0)

        if (m_timer <= TIME_POPUP) {
            m_state = AnimState::CenterPopUp;
            float t = m_timer / TIME_POPUP;
            // Ease-Out Cubic: 出現時は勢いよく、止まる直前は滑らかに
            t = 1.0f - std::pow(1.0f - t, 3.0f);
            m_currentScale = Vector3(t, t, 1.0f);
            m_currentPos = centerPos;
        }
        else if (m_timer <= TIME_WAIT) {
            m_state = AnimState::CenterWait;
            m_currentScale = Vector3(1.0f, 1.0f, 1.0f);
            m_currentPos = centerPos;
        }
        else if (m_timer <= TIME_MOVE) {
            m_state = AnimState::MoveToCorner;
            float t = (m_timer - TIME_WAIT) / (TIME_MOVE - TIME_WAIT);
            // Smoothstep: 動き出しと停止の両方を滑らかにする
            t = t * t * (3.0f - 2.0f * t);

            // スケールを 1.0 から 0.8 へ縮小
            float s = 1.0f + (0.8f - 1.0f) * t;
            m_currentScale = Vector3(s, s, 1.0f);

            // 座標の補間（アンチエイリアスや抖動防止のため四捨五入）
            float px = centerPos.x + (targetPos.x - centerPos.x) * t;
            float py = centerPos.y + (targetPos.y - centerPos.y) * t;
            m_currentPos.x = std::round(px);
            m_currentPos.y = std::round(py);
            m_currentPos.z = 0.0f;
        }
        else {
            m_state = AnimState::Fixed;
            m_currentScale = Vector3(0.8f, 0.8f, 1.0f);
            m_currentPos = targetPos;
        }
    }

    void Draw() {
		// 非表示状態なら描画しない
        if (m_state == AnimState::Hidden) return;
		// インデックス０がターン１、インデックス４がターン５に対応
		// ターン数が０の場合は脱出提示画像を表示する
        // ニアレストネイバー（点サンプリング）を強制：整数倍でないスケーリング時のピクセルぼけを防止
        Renderer::SetUISamplerMode(true);

		if (m_currentTurn <= 0) {
			// ターン数が0の場合、脱出提示画像を表示する
			if (m_escapePromptSprite) {
				MATERIAL mtrl;
				mtrl.Diffuse = Color(1.0f, 1.0f, 1.0f, 1.0f);
				mtrl.TextureEnable = TRUE;
				m_escapePromptSprite->ModifyMtrl(mtrl);
				m_escapePromptSprite->Draw(m_currentScale, Vector3(0, 0, 0), m_currentPos);
			}
		}
		else {
			// 通常の数字表示
			int index = m_currentTurn - 1;
			if (index >= 0 && index < m_numberSprites.size()) {
				MATERIAL mtrl;
				mtrl.Diffuse = Color(1.0f, 1.0f, 1.0f, 1.0f);
				mtrl.TextureEnable = TRUE;
				m_numberSprites[index]->ModifyMtrl(mtrl);
				m_numberSprites[index]->Draw(m_currentScale, Vector3(0, 0, 0), m_currentPos);
			}
		}

        Renderer::SetUISamplerMode(false);
    }

private:
	// ターンカウンター用のスプライト配列
    std::vector<std::unique_ptr<CSprite>> m_numberSprites;
	//脱出提示用のスプライト
    std::unique_ptr<CSprite> m_escapePromptSprite;
	// 現在のターン数
    int m_currentTurn = 5;
	// アニメーションの状態管理
    AnimState m_state = AnimState::Hidden;
    float m_timer = 0.0f;
    Vector3 m_currentPos{ 0, 0, 0 };
    Vector3 m_currentScale{ 1, 1, 1 };
};