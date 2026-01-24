#pragma once
#include <string>
#include <memory>
#include "../system/CSprite.h"
#include "../Application.h"
#include "../system/Transform.h"
#include <iostream>

class TurnCutin
{
public:
	enum class AnimState {
		Idle,
		FadeIn,
		Wait,
		FadeOut
	};

	TurnCutin() {};

	void Init() {
		m_playerSprite = std::make_unique<CSprite>(684, 364, "assets/texture/ui_cutin_PlayerPhase.png");
		m_enemySprite = std::make_unique<CSprite>(684, 364, "assets/texture/ui_cutin_EnemyPhase.png");

		m_srt.scale = Vector3(0.0f, 0.0f, 1.0f);
		m_srt.rot = Vector3(0.0f, 0.0f, 0.0f);
		m_state = AnimState::Idle;
		m_timer = 0.0f;
	}

	void PlayCutinAnimation(std::string type)
	{
		m_timer = 0.0f;
		m_srt.scale = Vector3(0.0f, 0.0f, 1.0f);
		m_srt.pos = Vector3(0.0f, 0.0f, 0.0f);
		
		m_state = AnimState::FadeIn;
		
		if(type =="Player Phase")
		{
			m_currentSprite = m_playerSprite.get();
		}
		else if(type=="Enemy Phase")
		{
			m_currentSprite = m_enemySprite.get();
		}
		else
		{
			m_currentSprite = nullptr;
		}
		m_srt.scale = Vector3(0.0f, 0.0f, 1.0f);
	}

	void Update(uint64_t dt)
	{
		if (m_state == AnimState::Idle || m_currentSprite == nullptr) return;

		float deltaSeconds = static_cast<float>(dt) / 1000.0f;
		m_timer += deltaSeconds;

		float screenW = (float)Application::GetWidth();
		float screenH = (float)Application::GetHeight();

		float centerX = screenW / 2.0f;
		float centerY = screenH / 2.0f;

		m_srt.pos = Vector3(centerX, centerY, 0.0f);

		float currentScale = 1.0f;
		float currentAlpha = 1.0f;


		// 0s(開始) -> 0.5s(入場完了)
		if (m_timer < m_timerPhase1) {
			float t = m_timer / m_timerPhase1;
			//easeOut使う、先ずは早い動き、後でゆっくり
			t = 1.0f - pow(1.0f - t, 2.0f);//powは冪乗,(1.0f-t)の3乗

			currentScale = std::lerp(0.0f, 1.0f, t); // 0 -> 1
			currentAlpha = std::lerp(0.0f, 1.0f, t); // 透明 -> 不透明
			m_state = AnimState::FadeIn;
		}
		//  0.5s(入場完了) -> 2.5s(退場開始)
		else if (m_timer < m_timerPhase2)
		{
			currentScale = 1.0f;
			currentAlpha = 1.0f;
			m_state = AnimState::Wait;
		}
		//2.5s(退場開始) -> 3.0s(終わり)
		else if (m_timer < m_timerPhase3)
		{
			float fadeOutDuration = m_timerPhase3 - m_timerPhase2;

			float t = (m_timer - m_timerPhase2) / fadeOutDuration;

			//easeIn使う、先ずはゆっくり、後で早い動き
			t = pow(t, 2.0f);
		
			currentScale = std::lerp(1.0f, 0.0f, t); // 1 -> 0
			currentAlpha = std::lerp(1.0f, 0.0f, t); // 不透明 -> 透明
			m_state = AnimState::FadeOut;
		}
		else
		{
			//アニメ終了
			m_state = AnimState::Idle;
			return;
		}

		m_srt.scale = Vector3(currentScale, currentScale, 1.0f);

		MATERIAL mtrl;
		mtrl.Ambient = Color(0, 0, 0, 0);
		mtrl.Diffuse = Color(1.0f, 1.0f, 1.0f, currentAlpha);
		mtrl.Specular = Color(0, 0, 0, 0);
		mtrl.Emission = Color(0, 0, 0, 0);
		mtrl.Shiness = 0;
		mtrl.TextureEnable = TRUE;

		m_currentSprite->ModifyMtrl(mtrl);


	}

	void Draw()
	{
		if (m_state != AnimState::Idle && m_currentSprite != nullptr)
		{
			m_currentSprite->Draw(this->m_srt.scale, this->m_srt.rot, this->m_srt.pos);
		}
	}

	bool IsAnimating() const { return m_state != AnimState::Idle; }
private:
	std::unique_ptr<CSprite> m_playerSprite;
	std::unique_ptr<CSprite> m_enemySprite;

	CSprite* m_currentSprite = nullptr;
	AnimState m_state = AnimState::Idle;
	SRT m_srt;
	float m_targetX = 0.0;
	float m_timer = 0.0f;

	const float m_timerPhase1 = 0.2f; // 入場完了
	const float m_timerPhase2 = 0.6f; // 退場開始
	const float m_timerPhase3 = 0.8f; // 終わり
};