#pragma once
#include <Audio.h> 
#include <map>
#include <string>
#include <memory>
#include <iostream>

// =========================================================
// AudioManager クラス (Singleton)
// DirectX AudioEngine をラップし、BGMのクロスフェードや
// SE（効果音）の再生状態を一括管理するサウンドシステム
// =========================================================
class AudioManager
{
public:
	static AudioManager& GetInstance() {
		static AudioManager instance;
		return instance;
	}

	// ---------------------------------------------------------
	// ライフサイクル (Lifecycle)
	// ---------------------------------------------------------
	void Init();
	void Update(float dt);
	void Suspend(); // バックグラウンド移行時の一時停止
	void Resume();  // フォアグラウンド復帰時の再開

	// ---------------------------------------------------------
	// BGM 制御 (Background Music Control)
	// ---------------------------------------------------------
	// BGMの再生（既存曲がある場合は自動的にクロスフェードを行う）
	void PlayBGM(const std::string& name, bool loop = true, float fadeTime = 1.0f);
	void StopBGM(float fadeTime = 1.0f);

	// ---------------------------------------------------------
	// SE 制御 (Sound Effects Control)
	// ---------------------------------------------------------
	void LoadSE(const std::string& name, const std::wstring& path);

	// SEの再生 (Fire-and-forget 方式：単発の音の再生に特化)
	void PlaySE(const std::string& name, float volume = 1.0f, float pitch = 0.0f, float pan = 0.0f);

private:
	AudioManager() = default;
	~AudioManager();
	AudioManager(const AudioManager&) = delete;
	AudioManager& operator=(const AudioManager&) = delete;

	// =========================================================
	// メンバー変数
	// =========================================================
	std::unique_ptr<DirectX::AudioEngine> m_audEngine;
	std::map<std::string, std::unique_ptr<DirectX::SoundEffect>> m_soundEffects;

	// --- BGM 再生インスタンス ---
	std::unique_ptr<DirectX::SoundEffectInstance> m_currentBGM; // 再生中・フェードアウト中
	std::unique_ptr<DirectX::SoundEffectInstance> m_nextBGM;    // 準備中・フェードイン中

	// --- フェード制御ステート ---
	bool m_isCrossFading = false;
	bool m_isStopping = false;
	float m_fadeTimer = 0.0f;
	float m_fadeDuration = 1.0f;
};