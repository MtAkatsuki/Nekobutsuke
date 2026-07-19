#include "IntroDirector.h"

#include "../../System/Camera.h"
#include "../../Core/GameContext.h"
#include "../../UI/Component/TurnCounter.h"
#include "../../UI/System/GameUIManager.h"
#include "../../UI/Component/DialogueUI.h"
#include "../../Actor/Character/Player.h"
#include "../../Actor/Character/Ally.h"

namespace {
    const float CAMERA_ARRIVAL_TOLERANCE = 0.2f;  // カメラ到達判定の許容誤差
    const float CAMERA_HOVER_DURATION = 0.5f;  // BaseView到達後の待機時間
    const float INTRO_FOCUS_RADIUS = 14.0f;  // 開場特写の距離（通常のTargetFocus=17より寄せる）
}

void IntroDirector::Start() {
    if (m_state != State::Idle) return;   // 二重開始ガード
    m_state = State::TurnCounterFlying;

    // 演出中の誤操作を防ぐため入力をロック
    if (m_context && m_context->GetUIManager()) {
        m_context->GetUIManager()->SetEventBlock(true);
    }
}

bool IntroDirector::HasCameraArrived() const {
    Camera* camera = m_context ? m_context->GetCamera() : nullptr;
    if (!camera) return false;
    Vector3 diff = camera->GetLookat() - camera->GetTargetLookAt();
    return diff.Length() < CAMERA_ARRIVAL_TOLERANCE;
}

void IntroDirector::Update(float deltaSeconds, bool allyTalked) {
    if (m_state == State::Idle || m_state == State::Finished) {
        return;
    }

    Camera* camera = m_context ? m_context->GetCamera() : nullptr;

    // シンプルな状態遷移で導入演出を制御
    if (m_state == State::TurnCounterFlying) {
        if (m_turnCounter && !m_turnCounter->IsAnimating()) {
            m_state = State::CameraToAlly;
            Ally* ally = m_context ? m_context->GetAlly() : nullptr;
            if (ally && camera) {
                // 味方へ視点を移動
                camera->ChangeState(CameraState::TargetFocus, ally->GetSRT().pos);
                camera->SetTargetRadius(INTRO_FOCUS_RADIUS);
            }
        }
    }
    else if (m_state == State::CameraToAlly) {
        if (HasCameraArrived()) {
            m_state = State::WaitingAllyDialogue;
        }
    }
    else if (m_state == State::WaitingAllyDialogue) {
        // セリフ演出の完了後にカメラをBaseViewへ戻す
        DialogueUI* dialogue = m_context ? m_context->GetDialogueUI() : nullptr;
        if (allyTalked && dialogue && !dialogue->IsShowing()) {
            m_state = State::CameraToBase;
            m_hoverTimer = 0.0f;
            if (camera) {
                camera->ChangeState(CameraState::BaseView);
            }
        }
    }

    // === BaseViewへ移動し、一定時間待機 ===
    else if (m_state == State::CameraToBase) {
        if (HasCameraArrived()) {
            m_hoverTimer += deltaSeconds;

            if (m_hoverTimer >= CAMERA_HOVER_DURATION) {
                m_hoverTimer = 0.0f;
                m_state = State::CameraToPlayer;
                Player* player = m_context ? m_context->GetPlayer() : nullptr;
                if (player && camera) {
                    // プレイヤー追従カメラへ復帰
                    camera->ChangeState(CameraState::Tracking, player->GetSRT().pos);
                }
            }
        }
    }
    else if (m_state == State::CameraToPlayer) {
        if (HasCameraArrived()) {
            m_state = State::Finished;
            // 導入演出が完了。入力を有効化し、行動UIを表示
            if (m_context && m_context->GetUIManager()) {
                m_context->GetUIManager()->SetEventBlock(false);
            }
        }
    }
}