#include "AudioManager.h"

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
        // オーディオデバイスが見つからないなどの理由で初期化に失敗した場合
        std::cerr << "[AudioManager] Failed to initialize AudioEngine. No audio device?" << std::endl;
        return; // オーディオ機能は無効になるが、クラッシュは回避する
    }

    // 2. BGMリソースの読み込み (wavファイルの存在を前提とする)
    // ※ assets/sound/ ディレクトリにファイルが配置されていることを確認してください
    try {
        // LoadSound: ここでは4つのBGMをハードコーディングで読み込む（動的ロードへの変更も可能）
        m_soundEffects["Title"] = std::make_unique<DirectX::SoundEffect>(m_audEngine.get(), L"assets/sound/bgm_title.wav");
        m_soundEffects["Game"] = std::make_unique<DirectX::SoundEffect>(m_audEngine.get(), L"assets/sound/bgm_game.wav");
        m_soundEffects["Clear"] = std::make_unique<DirectX::SoundEffect>(m_audEngine.get(), L"assets/sound/bgm_clear.wav");
        m_soundEffects["Over"] = std::make_unique<DirectX::SoundEffect>(m_audEngine.get(), L"assets/sound/bgm_over.wav");

        std::cerr << "[AudioManager] Audio resources loaded successfully." << std::endl;
    }
    catch (...) {
        // ファイルパスが正しくない、またはファイルが破損している場合
        std::cerr << "[AudioManager] Failed to load wave files! Check file paths." << std::endl;
    }
}

void AudioManager::Update(float dt) {
    if (!m_audEngine) return;

    // オーディオエンジンの状態更新
    if (!m_audEngine->Update()) {
        // オーディオデバイスのロスト（例：イヤホンの抜き差し）発生時、リセットを試行
        if (m_audEngine->IsCriticalError()) {
            std::cerr << "[AudioManager] Critical Error! Resetting..." << std::endl;
            // 簡易的な対応：本来は再初期化(Init)が望ましいが、低リスク化のためリセットのみ実行
            m_audEngine->Reset();
        }
    }

    // フェード（クロスフェード / フェードアウト）ロジックの処理
    if (m_isCrossFading || m_isStopping) {
        m_fadeTimer += dt;
        float progress = m_fadeTimer / m_fadeDuration;
        if (progress > 1.0f) progress = 1.0f;

        // 1. 現在のBGMのフェードアウト (音量: 最大 -> 0)
        if (m_currentBGM) {
            float vol = MAX_VOLUME * (1.0f - progress);
            m_currentBGM->SetVolume(vol);
        }

        // 2. 次のBGMのフェードイン (音量: 0 -> 最大)
        if (m_nextBGM && !m_isStopping) {
            float vol = MAX_VOLUME * progress;
            m_nextBGM->SetVolume(vol);
        }

        // 3. 終了判定とステートの切り替え
        if (progress >= 1.0f) {
            // 旧BGMの停止と破棄
            if (m_currentBGM) {
                m_currentBGM->Stop();
                m_currentBGM.reset();
            }

            // 次のBGMがある場合、それを現在のBGMとして昇格させる
            if (m_nextBGM) {
                m_currentBGM = std::move(m_nextBGM);
                m_currentBGM->SetVolume(MAX_VOLUME); // 音量を完全に固定させる
            }

            // 各種フラグのリセット
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

    // 新しい再生インスタンスの生成
    auto effect = m_soundEffects[name].get();
    m_nextBGM = effect->CreateInstance();

    if (m_nextBGM) {
        // インスタンスの設定
        // GameSceneはループ、その他は非ループ（引数 loop で制御）


         // 初期音量を0に設定（フェードインの準備）
        m_nextBGM->SetVolume(0.0f);
        m_nextBGM->Play(loop);

        // フェードパラメータの設定
        m_fadeDuration = fadeTime;
        m_fadeTimer = 0.0f;
        m_isCrossFading = true;
        m_isStopping = false;
    }
}

void AudioManager::StopBGM(float fadeTime) {
    // すでに停止中、またはフェードアウト中の場合は何もしない
    if (!m_currentBGM || m_isStopping) return;

    m_fadeDuration = fadeTime;
    m_fadeTimer = 0.0f;
    m_isStopping = true;    // 停止専用（フェードアウトのみ）モードとしてマーク
    m_isCrossFading = true; // 既存のフェードロジックを再利用
    m_nextBGM.reset();      // 次の曲が予約されていないことを保証
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
        m_seMap[name] = std::make_unique<DirectX::SoundEffect>(m_audEngine.get(), path.c_str());
        std::cout << "[AudioManager] SE Loaded: " << name << std::endl;
    }
    catch (...) {
        // ファイルの読み込み失敗やフォーマットエラーのハンドリング
        std::cerr << "[AudioManager] Failed to load SE!" << std::endl;
    }
}

void AudioManager::PlaySE(const std::string& name, float volume, float pitch, float pan) {
    if (!m_audEngine) return;
    auto it = m_seMap.find(name);
    if (it != m_seMap.end()) {
        // Playメソッドは Fire-and-forget 形式のため、短い効果音に適している
        it->second->Play(volume, pitch, pan);
        /*Pitch（ピッチ / 音高）
        役割： オーディオ再生時の音高（周波数）および再生速度を調整します。 
        値の範囲： 通常は 0.5f ～ 2.0f の浮動小数点数で指定します。
        効果：
        pitch = 1.0f：オリジナルの音高（通常の再生速度）。
        pitch > 1.0f：音高が高くなり、再生速度も速くなります（例：1.5f は音高が50%上昇）。
        pitch < 1.0f：音高が低くなり、再生速度も遅くなります（例：0.8f は音高が20%低下）。

        Pan（パン / 定位）
        役割： 左右のスピーカー（チャンネル）間の音量バランスを制御し、音の聞こえる位置を調整します
        値の範囲： 通常は -1.0f ～ +1.0f で指定します。 
        効果：
        pan = -1.0f：左チャンネルのみから再生（完全に左）。
        pan = 0.0f：左右均等に再生（センター定位）。
        pan = +1.0f：右チャンネルのみから再生（完全に右）。*/
    }
}