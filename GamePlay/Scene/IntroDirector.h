#pragma once

// =========================================================
// IntroDirector
//
// 導入シネマティックの進行制御を担当するクラス。
// 演出の流れのみを管理し、ゲームロジックは保持しない。
// 依存先はすべて GameContext 経由で取得し、
// TurnCounter のみシーン所有オブジェクトのためコンストラクタで受け取る。
// =========================================================
class GameContext;
class TurnCounter;

class IntroDirector {
public:
    IntroDirector(GameContext* context, TurnCounter* turnCounter)
        : m_context(context), m_turnCounter(turnCounter) {
    }

    // 演出開始：入力をロックし、ターンカウンター演出の完了待ちへ遷移
    void Start();

    // 毎フレーム更新。
    // allyTalked = シーン側で味方のセリフ演出が開始済みか
    void Update(float deltaSeconds, bool allyTalked);

    bool IsIdle()     const { return m_state == State::Idle; }
    bool IsFinished() const { return m_state == State::Finished; }

private:
    enum class State {
        Idle,                // 待機
        TurnCounterFlying,   // ターンカウンターUIの演出中
        CameraToAlly,         // カメラを味方（ネズミ）へ移動
        WaitingAllyDialogue,  // 味方のセリフ演出完了待ち
        CameraToBase,         // 全体俯瞰（BaseView）へ戻る
        CameraToPlayer,       // プレイヤーへ戻る
        Finished              // 導入演出完了
    };

    // カメラが目標注視点へ到達したか（Lerp収束判定）
    bool HasCameraArrived() const;

    GameContext* m_context = nullptr;      
    TurnCounter* m_turnCounter = nullptr;  
    State m_state = State::Idle;
    float m_hoverTimer = 0.0f;             // 俯瞰表示の待機時間
};