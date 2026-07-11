#include "GameResultJudge.h"

#include "../../Core/GameContext.h"
#include "../../Core/DebugLog.h"
#include "../../System/Camera.h"
#include "../../System/scenemanager.h"
#include "../../System/BackgroundTransition.h"
#include "../../GamePlay/Manager/EnemyManager.h"
#include "../../Actor/Character/Player.h"
#include "../../Actor/Character/Ally.h"

namespace {
	const float GAMEOVER_WAIT_DURATION = 1.0f;           // 敗北演出後、シーン遷移までの待機時間
	const float GAMECLEAR_WAIT_DURATION = 1.0f;          // // 勝利確定後の待機時間
	constexpr float FADE_IN_OUT_DURATION = 1000.0f;      // フェードイン・アウトの合計時間（ms）
	constexpr float BackgroundTransitionTime = 4000.0f;  // 背景遷移のスクロール時間（ms）
}

void GameResultJudge::Update(float deltaSeconds) {
	if (m_isSceneChanging) return;

	// 敗北処理を優先。敗北フロー中は勝利判定を行わない
	if (ProcessGameOverFlow(deltaSeconds)) return;

	ProcessGameClearFlow(deltaSeconds);
}

bool GameResultJudge::IsGameOverCondition() const {
	// 敗北条件①：味方が死亡（脱出中・脱出完了時は対象外）
	bool isAllyDead = false;
	if (m_context && m_context->GetAlly()) {
		bool hpDepleted = (m_context->GetAlly()->GetHP() <= 0);
		bool isSafe = (m_context->GetAlly()->IsEscaping() || m_context->GetAlly()->IsEscapeDone());
		isAllyDead = (hpDepleted && !isSafe);
	}

	// 敗北条件②：プレイヤーの死亡
	Player* player = m_context ? m_context->GetPlayer() : nullptr;
	bool isPlayerDead = (player && player->GetHP() <= 0);

	return (isAllyDead || isPlayerDead);
}

bool GameResultJudge::ProcessGameOverFlow(float deltaSeconds) {
	if (!IsGameOverCondition() && !m_isGameOverProcessing) {
		return false;
	}

	// 初回のみ待機タイマーを開始し、敗北演出の表示時間を確保
	if (!m_isGameOverProcessing) {
		m_isGameOverProcessing = true;
		m_gameOverTimer = 0.0f;
		DBG_ERROR("[GameResultJudge] GameOver detected! Waiting for animation...");
	}

	m_gameOverTimer += deltaSeconds;

	// KillCam終了後にシーン遷移（演出完了を優先）
	bool cineFinished = !m_context || !m_context->GetCamera() || !m_context->GetCamera()->IsCinematic();
	if (m_gameOverTimer >= GAMEOVER_WAIT_DURATION && cineFinished) {
		m_isSceneChanging = true;

		SceneManager::GetInstance().SetCurrentScene(
			"GameOverScene",
			std::make_unique<BackgroundTransition>(FADE_IN_OUT_DURATION, BackgroundTransitionTime)
		);
	}
	return true;
}

bool GameResultJudge::IsGameClearCondition() const {
	// 勝利ルートA：盤面の敵をすべて排除した
	bool isEnemyAnnihilated = (m_context && m_context->GetEnemyManager() &&
		m_context->GetEnemyManager()->AreAllEnemiesDead());

	// 勝利ルートB：指定ターンを凌ぎきり、プレイヤーが脱出地点に到達（勝利アニメが完了）した
	Player* player = m_context ? m_context->GetPlayer() : nullptr;
	bool isSurvivalEscaped = (player && player->IsCelebrationDone());

	return (isEnemyAnnihilated || isSurvivalEscaped);
}

bool GameResultJudge::IsFieldBusyForClear() const {
	// 勝利条件を満たしていても「最後の攻撃」や「敵の消滅」が画面上で完了するまでは遷移させない

	// 1. プレイヤーがアクション（移動・攻撃）を消化しているか
	Player* player = m_context ? m_context->GetPlayer() : nullptr;
	if (player) {
		PlayerState pState = player->GetState();
		if (pState == PlayerState::ANIM_MOVE || pState == PlayerState::ANIM_ATTACK) {
			return true;
		}
	}

	// 2. 敵の消滅アニメーションなどが残っていないか
	if (m_context && m_context->GetEnemyManager()) {
		if (m_context->GetEnemyManager()->IsAnyEnemyDying() ||
			m_context->GetEnemyManager()->IsAnyEnemyAnimating()) {
			return true;
		}
	}

	return false;
}

void GameResultJudge::ProcessGameClearFlow(float deltaSeconds) {
	if (!IsGameClearCondition()) {
		m_gameClearTimer = 0.0f;
		return;
	}

	if (IsFieldBusyForClear()) {
		return;
	}

	// すべてのアニメーションが終わった後、クリアの余韻（ウェイト）カウントを開始
	m_gameClearTimer += deltaSeconds;

	if (m_gameClearTimer >= GAMECLEAR_WAIT_DURATION && !m_isSceneChanging) {
		m_isSceneChanging = true;

		SceneManager::GetInstance().SetCurrentScene(
			"GameClearScene",
			std::make_unique<BackgroundTransition>(FADE_IN_OUT_DURATION, BackgroundTransitionTime)
		);
	}
}