#pragma once
class GameContext;

class GameResultJudge {
public:
    explicit GameResultJudge(GameContext* context) : m_context(context) {}

    // 毎フレーム勝敗を判定し、確定後は余韻時間を経てシーン遷移まで実行する
    void Update(float deltaSeconds);

    // 現在、敗北条件を満たしているか（ターン進行の抑止に使用）
    bool IsGameOverCondition() const;

    // 敗北演出の待機中か
    bool IsGameOverProcessing() const { return m_isGameOverProcessing; }

    // シーン遷移開始済みか（以降の入力を無効化）
    bool IsSceneChanging() const { return m_isSceneChanging; }

private:
    // 敗北フローを進める。敗北処理中なら true（勝利判定を抑止する）
    bool ProcessGameOverFlow(float deltaSeconds);
    // 勝利フローを進め、余韻の待機を経てクリアシーンへ遷移する
    void ProcessGameClearFlow(float deltaSeconds);

    // 勝利条件（敵全滅 または 脱出達成）を満たしているか
    bool IsGameClearCondition() const;
    // 攻撃・消滅などの演出が残っていて、遷移を待つべきか
    bool IsFieldBusyForClear() const;

    GameContext* m_context = nullptr;   // 非所有

    bool  m_isGameOverProcessing = false;
    bool  m_isSceneChanging = false;
    float m_gameOverTimer = 0.0f;
    float m_gameClearTimer = 0.0f;
};