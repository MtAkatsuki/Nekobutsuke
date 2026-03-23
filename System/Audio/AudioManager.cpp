#include "AudioManager.h"

namespace {
    // サウンドミキシング定数 (Audio Mixing Constants)
    const float MAX_BGM_VOLUME = 0.5f; // BGMの最大音量（音割れ防止・SEを際立たせるためのヘッドルーム確保）
}

void AudioManager::Init() {
    // 1. オーディオエンジンの生成
    DirectX::AUDIO_ENGINE_FLAGS eflags = DirectX::AudioEngine_Default;
#ifdef _DEBUG
    eflags |= DirectX::AudioEngine_Debug;
#endif

    try {
        m_audEngine = std::make_unique<DirectX::AudioEngine>(eflags);
    }
    catch (...) {
        std::cerr << "[AudioManager] Failed to initialize AudioEngine. No audio device?" << std::endl;
        return;
    }

    // 2. BGMリソースの読み込み (wavファイルの存在を前提とする)
    try {
        m_soundEffects["Title"] = std::make_unique<DirectX::SoundEffect>(m_audEngine.get(), L"Assets/sound/bgm_title.wav");
        m_soundEffects["Game"] = std::make_unique<DirectX::SoundEffect>(m_audEngine.get(), L"Assets/sound/bgm_game.wav");
        m_soundEffects["Clear"] = std::make_unique<DirectX::SoundEffect>(m_audEngine.get(), L"Assets/sound/bgm_clear.wav");
        m_soundEffects["Over"] = std::make_unique<DirectX::SoundEffect>(m_audEngine.get(), L"Assets/sound/bgm_over.wav");
        m_soundEffects["DigSE"] = std::make_unique<DirectX::SoundEffect>(m_audEngine.get(), L"Assets/sound/DigSE.wav");
        std::cerr << "[AudioManager] Audio resources loaded successfully." << std::endl;
    }
    catch (...) {
        std::cerr << "[AudioManager] Failed to load wave files! Check file paths." << std::endl;
    }
}

void AudioManager::Update(float dt) {
    if (!m_audEngine) return;

    if (!m_audEngine->Update()) {
        // オーディオデバイスのロスト（例：イヤホンの抜き差し）発生時、リセットを試行
        if (m_audEngine->IsCriticalError()) {
            std::cerr << "[AudioManager] Critical Error! Resetting..." << std::endl;
            m_audEngine->Reset();
        }
    }

    // クロスフェード・フェードアウトのロジック
    if (m_isCrossFading || m_isStopping) {
        m_fadeTimer += dt;
        float progress = m_fadeTimer / m_fadeDuration;
        if (progress > 1.0f) progress = 1.0f;

        // 現在のBGMのフェードアウト
        if (m_currentBGM) {
            float vol = MAX_BGM_VOLUME * (1.0f - progress);
            m_currentBGM->SetVolume(vol);
        }

        // 次のBGMのフェードイン
        if (m_nextBGM && !m_isStopping) {
            float vol = MAX_BGM_VOLUME * progress;
            m_nextBGM->SetVolume(vol);
        }

        // フェード完了時のステート遷移
        if (progress >= 1.0f) {
            if (m_currentBGM) {
                m_currentBGM->Stop();
                m_currentBGM.reset();
            }

            // 次のBGMがある場合、それを現在のBGMとして昇格させる
            if (m_nextBGM) {
                m_currentBGM = std::move(m_nextBGM);
                m_currentBGM->SetVolume(MAX_BGM_VOLUME); // 音量を完全に固定させる
            }

            m_isCrossFading = false;
            m_isStopping = false;
        }
    }
}

void AudioManager::PlayBGM(const std::string& name, bool loop, float fadeTime) {
    if (!m_audEngine) return;
    if (m_soundEffects.find(name) == m_soundEffects.end()) {
        std::cerr << "[AudioManager] Sound not found: " << name << std::endl;
        return;
    }

    std::cerr << "[AudioManager] Request PlayBGM: " << name << std::endl;

    // すでにフェード中の場合は、前回の処理を強制終了して新しいフェードを開始する
    if (m_isCrossFading) {
        if (m_currentBGM) m_currentBGM->Stop();
        m_currentBGM = std::move(m_nextBGM); // フェードイン中だったものを現在のBGMに繰り上げる
        m_nextBGM.reset();
    }

    auto effect = m_soundEffects[name].get();
    m_nextBGM = effect->CreateInstance();

    if (m_nextBGM) {
        // GameSceneはループ、その他は非ループ（引数 loop で制御）
        m_nextBGM->SetVolume(0.0f);// 初期音量を0に設定（フェードインの準備）
        m_nextBGM->Play(loop);

        m_fadeDuration = fadeTime;
        m_fadeTimer = 0.0f;
        m_isCrossFading = true;
        m_isStopping = false;
    }
}

void AudioManager::StopBGM(float fadeTime) {
    if (!m_currentBGM || m_isStopping) return;

    m_fadeDuration = fadeTime;
    m_fadeTimer = 0.0f;
    m_isStopping = true;
    m_isCrossFading = true;
    m_nextBGM.reset();
}

void AudioManager::Suspend() {
    if (m_audEngine) m_audEngine->Suspend();
}

void AudioManager::Resume() {
    if (m_audEngine) m_audEngine->Resume();
}

AudioManager::~AudioManager() {
    if (m_audEngine) m_audEngine->Suspend();
    m_currentBGM.reset();
    m_nextBGM.reset();
    m_soundEffects.clear();
    m_audEngine.reset();
}


void AudioManager::LoadSE(const std::string& name, const std::wstring& path) {
    if (!m_audEngine) return;
    try {
        m_soundEffects[name] = std::make_unique<DirectX::SoundEffect>(m_audEngine.get(), path.c_str());
        std::cout << "[AudioManager] SE Loaded: " << name << std::endl;
    }
    catch (...) {
        std::cerr << "[AudioManager] Failed to load SE!" << std::endl;
    }
}

void AudioManager::PlaySE(const std::string& name, float volume, float pitch, float pan) {
    if (!m_audEngine) return;

    auto it = m_soundEffects.find(name);
    if (it != m_soundEffects.end()) {
        // Playメソッドは Fire-and-forget 形式のため、短い効果音に適している
        it->second->Play(volume, pitch, pan);//Pitch（ピッチ / 音高）、Pan（パン / 音の定位）について：
    }
    else {
        std::cerr << "[AudioManager] ERROR: SE Not Found in map: " << name << std::endl;
    }
}