#pragma once
#include <string>
#include <memory>
#include <cmath>
#include "../../System/CSprite.h"
#include "../../Core/Application.h"
#include "../../System/Transform.h"

// =========================================================
// TurnCutin クラス
// ターン開始時（Player Phase / Enemy Phase）の画面カットイン演出を制御する
// =========================================================
class TurnCutin {
public:
	enum class AnimState {
		Idle,
		FadeIn,
		Wait,
		FadeOut
	};

	TurnCutin() = default;

	// ---------------------------------------------------------
	// ライフサイクル (Lifecycle)
	// ---------------------------------------------------------
	void Init() {
		m_playerSprite = std::make_unique<CSprite>(TEX_WIDTH, TEX_HEIGHT, "Assets/texture/ui_cutin_PlayerPhase.png");
		m_enemySprite = std::make_unique<CSprite>(TEX_WIDTH, TEX_HEIGHT, "Assets/texture/ui_cutin_EnemyPhase.png");

		m_srt.scale = Vector3(0.0f, 0.0f, 1.0f);
		m_srt.rot = Vector3(0.0f, 0.0f, 0.0f);
		m_state = AnimState::Idle; 
		m_timer = 0.0f;
	}

	void PlayCutinAnimation(const std::string& type) {
		m_timer = 0.0f;
		m_srt.scale = Vector3(0.0f, 0.0f, 1.0f);
		m_srt.pos = Vector3(0.0f, 0.0f, 0.0f);

		if (type == "Player Phase") m_activeSprite = m_playerSprite.get();
		else if (type == "Enemy Phase") m_activeSprite = m_enemySprite.get();

		m_state = AnimState::FadeIn;
	}

	void Update(uint64_t dt) {
		if (m_state == AnimState::Idle) return;

		float deltaSeconds = static_cast<float>(dt) / 1000.0f;
		m_timer += deltaSeconds;

		// 状态に応じたアニメーション計算
		if (m_timer < TIME_FADE_IN) {
			// フェーズ1：素早く拡大しながら出現
			float t = m_timer / TIME_FADE_IN;
			t = 1.0f - std::pow(1.0f - t, 2.0f);

			m_currentScale = std::lerp(0.0f, 1.0f, t);
			m_currentAlpha = std::lerp(0.0f, 1.0f, t);
			m_state = AnimState::FadeIn;
		}
		else if (m_timer < TIME_WAIT_END) {
			// フェーズ2：画面中央で停止し、プレイヤーにフェーズを認識させる
			m_currentScale = 1.0f;
			m_currentAlpha = 1.0f;
			m_state = AnimState::Wait;
		}
		else if (m_timer < TIME_FADE_OUT_END) {
			// フェーズ3：徐々に消滅 (Ease-In：最初はゆっくり、後半で一気に消える)
			float fadeOutDuration = TIME_FADE_OUT_END - TIME_WAIT_END;
			float t = (m_timer - TIME_WAIT_END) / fadeOutDuration;
			t = std::pow(t, 2.0f);

			m_currentScale = std::lerp(1.0f, 0.0f, t);
			m_currentAlpha = std::lerp(1.0f, 0.0f, t);
			m_state = AnimState::FadeOut;
		}
		else {
			m_state = AnimState::Idle;
			return;
		}

		m_srt.scale = Vector3(m_currentScale, m_currentScale, 1.0f);
	}

	void Draw() {
		if (m_state == AnimState::Idle || !m_activeSprite) return;

		Renderer::SetUISamplerMode(true);

		// 計算済みのアルファ値をマテリアルに適用
		MATERIAL mtrl;
		mtrl.Ambient = Color(0, 0, 0, 0);
		mtrl.Diffuse = Color(1.0f, 1.0f, 1.0f, m_currentAlpha);
		mtrl.Specular = Color(0, 0, 0, 0);
		mtrl.Emission = Color(0, 0, 0, 0);
		mtrl.TextureEnable = TRUE;
		m_activeSprite->ModifyMtrl(mtrl);

		// 画面中央へ描画
		Vector3 centerPos(Application::GetWidth() / 2.0f, Application::GetHeight() / 2.0f, 0.0f);
		m_activeSprite->Draw(m_srt.scale, m_srt.rot, centerPos);

		Renderer::SetUISamplerMode(false);
	}

	bool IsAnimating() const { return m_state != AnimState::Idle; }

private:
	// --- アニメーション用定数 ---
	static constexpr float TEX_WIDTH = 684.0f;
	static constexpr float TEX_HEIGHT = 364.0f;
	static constexpr float TIME_FADE_IN = 0.2f;
	static constexpr float TIME_WAIT_END = 0.6f;
	static constexpr float TIME_FADE_OUT_END = 0.8f;

	std::unique_ptr<CSprite> m_playerSprite;
	std::unique_ptr<CSprite> m_enemySprite;
	CSprite* m_activeSprite = nullptr;

	AnimState m_state = AnimState::Idle;
	SRT m_srt;

	float m_timer = 0.0f;
	float m_currentScale = 0.0f;
	float m_currentAlpha = 0.0f;
};