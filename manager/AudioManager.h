#pragma once
#include <Audio.h> // DirectXTK Audio
#include <map>
#include <string>
#include <memory>
#include <iostream>

class AudioManager
{
public:
	static AudioManager& GetInstance()
	{
		static AudioManager instance;
		return instance;
	}
	//AudioManagerの初期化とアセット読み込み
	void Init();

	void Update(float dt);

	// BGM（背景音楽）の再生
	// name: リソース名
	// loop: ループ再生するかどうか
	// fadeTime: クロスフェード/フェードインの時間（秒）
	void PlayBGM(const std::string& name, bool loop = true, float fadeTime = 1.0f);

	// 現在のBGMを停止（フェードアウト付き）
	void StopBGM(float fadeTime = 1.0f);

	// 一時停止・再開（アプリのバックグラウンド移行時などに使用）
	void Suspend();
	void Resume();

	// SEリソースの読み込み
	void LoadSE(const std::string& name, const std::wstring& path);
	// SEの再生 (Fire-and-forget 方式)
	void PlaySE(const std::string& name, float volume = 1.0f, float pitch = 0.0f, float pan = 0.0f);

private:
	//アセット保存用
	std::unique_ptr<DirectX::AudioEngine> m_audEngine;
	std::map<std::string, std::unique_ptr<DirectX::SoundEffect>> m_soundEffects;

	// 再生インスタンス
	std::unique_ptr<DirectX::SoundEffectInstance> m_currentBGM; // 現在再生中（またはフェードアウト中）
	std::unique_ptr<DirectX::SoundEffectInstance> m_nextBGM;    // 再生準備中（フェードイン中）

private:
	// フェード制御用変数
	bool m_isCrossFading = false;   // クロスフェード実行中フラグ
	float m_fadeTimer = 0.0f;       // フェード用タイマー
	float m_fadeDuration = 1.0f;    // フェードの総時間（継続時間）

	// 現在のBGMが「フェードアウトのみ」の状態かどうか（StopBGMに対応）
	bool m_isStopping = false;

	// 音量設定
	const float MAX_VOLUME = 0.5f; // 最大音量（音割れや騒音防止のため）
	// SE再生用
	std::map<std::string, std::unique_ptr<DirectX::SoundEffect>> m_seMap;
};