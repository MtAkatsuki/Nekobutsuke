#include "Ally.h"
#include "../../Core/GameContext.h"
#include "../../GamePlay/Manager/MapManager.h"
#include "../../System/MeshManager.h"
#include "../../System/Utility/WorldToScreen.h"
#include "../../Core/Application.h"
#include "../../UI/Component/DialogueUI.h"
#include "../../System/Audio/AudioManager.h"
#include "../../System/ModelRegistry.h"
#include "../../GamePlay/Manager/EffectManager.h"
#include "../../Core/DebugLog.h"
#include "../../GamePlay/Manager/EnemyManager.h"

namespace {
    // 演出・バランス用定数
    const int INITIAL_HP = 4;
    const int MAX_DIG_COUNT = 5;           // 採掘アニメーションの振り下ろし回数
    const float DIG_SPEED = 15.0f;         // 採掘アニメーションの速度
    const float DIG_HIT_ANGLE = 0.4f;      // 採掘エフェクトを発生させる閾値角度（ラジアン）
    const float FADE_OUT_SPEED = 1.0f;     // 脱出時のフェードアウト速度
    const float MODEL_SCALE = 0.7f;        // 味方モデルの表示スケール
    const float ESCAPE_POINT_HOLD = 0.6f;  // 採掘完了→脱出点出現後、台詞までの間（秒）
    const float ESCAPE_SPEAK_HOLD = 1.2f;  // 台詞表示→フェード開始までの間（秒）

    // 採掘演出の詳細パラメータ
    const float DIG_SWING_AMPLITUDE = 0.5f;    // ツルハシ振りの最大角度（ラジアン）
    const float DIG_SETTLE_TOLERANCE = 0.1f;   // 採掘終了とみなす静止角度の許容範囲
    const float DIG_SE_VOLUME = 2.6f;          // 採掘SEの音量
    const float RUBBLE_OFFSET_X = 0.5f;        // 瓦礫エフェクトの足元オフセット
    const int INTRO_DIG_COUNT = 5;     // 最初の一回特写時の振り下ろし回数（通常は MAX_DIG_COUNT）

    // --- 識別用アウトライン（呼吸パルス） ---
    const Color OUTLINE_PULSE_MIN = Color(0.0f, 0.78f, 1.0f, 0.0005f);  // シアン（.w = 幅）
    const Color OUTLINE_PULSE_MAX = Color(0.75f, 1.0f, 1.0f, 0.0005f);  // 白寄り（.w = 幅）
    const float OUTLINE_PULSE_SPEED = 2.5f;   // 呼吸速度（rad/s）

    // --- 危険警告（ロックオンされる時のジャンプ） ---
    const float ALARM_JUMP_SPEED = 15.0f;       // ジャンプ周期（rad/s、着地間隔 = π/速度）
    const float ALARM_JUMP_HEIGHT = 0.35f;     // ジャンプの最大高さ
    const int   ALARM_JUMP_COUNT = 3;           // 1セットで跳ぶ回数
    const float ALARM_JUMP_PAUSE = 1.0f;        // セット間の待機時間（秒）
    const float ALARM_DIALOGUE_DURATION = 3.0f;// 警告開始時の吹き出し表示時間（秒）
}

//Spawnファクトリー
std::unique_ptr<Ally> Ally::Spawn(GameContext* ctx, int gridX, int gridZ, const Vector3& worldPos) {
    auto a = std::unique_ptr<Ally>(new Ally(ctx));
    a->Init();
    a->SetGridPosition(gridX, gridZ);
    a->SetPosition(worldPos);
    a->UpdateWorldMatrix();
    return a;
}

void Ally::Init() 
{
    //モデル関連のソースをロード
    SetModelRenderer(ModelRegistry::RegisterModel(
        "ally_mesh", "Assets/model/character/Mouse/Mouse_Ally.obj", "Assets/model/character/Mouse"));
    m_shader = MeshManager::GetShader<CShader>("toonshader");


    //初期ステータスを設置
    m_maxHP = INITIAL_HP;
    m_currentHP = m_maxHP;
    m_maxMovePoints = 0;// 味方は自立移動しない
    m_currentMovePoints = m_maxMovePoints;
    m_srt.scale = Vector3(MODEL_SCALE, MODEL_SCALE, MODEL_SCALE);
    SetFacing(Direction::South);
    UpdateWorldMatrix();

    // 保護対象の識別用アウトライン（常時表示）
    SetOutlineOverride(OUTLINE_PULSE_MIN);

}

void Ally::Update(float deltaSeconds) {
    Unit::Update(deltaSeconds);

    // アウトラインの呼吸パルス（シアン⇔白寄りを往復）
    m_outlinePulseTimer += deltaSeconds;
    float pulse = (sinf(m_outlinePulseTimer * OUTLINE_PULSE_SPEED) + 1.0f) * 0.5f;  // 0～1
    SetOutlineOverride(Color::Lerp(OUTLINE_PULSE_MIN, OUTLINE_PULSE_MAX, pulse));

    if (m_isDeadFlying) {
        UpdateDeathFly(deltaSeconds);
        return;
    }

    // 脱出シーケンス：採掘完了(PointReveal) → 台詞(Speaking) → フェード開始(Fading)
    if (m_escapeState == EscapeState::PointReveal) {
        m_escapeSeqTimer += deltaSeconds;
        if (m_escapeSeqTimer >= ESCAPE_POINT_HOLD) {
            // 脱出点が出た後、ここで初めて台詞を出す（無限表示：フェード完了時に閉じる）
            if (m_context && m_context->GetDialogueUI())
                m_context->GetDialogueUI()->ShowDialogue(m_srt.pos, DialogueType::Escape, -1.0f);
            m_escapeState = EscapeState::Speaking;
            m_escapeSeqTimer = 0.0f;
        }
    }
    else if (m_escapeState == EscapeState::Speaking) {
        m_escapeSeqTimer += deltaSeconds;
        if (m_escapeSeqTimer >= ESCAPE_SPEAK_HOLD) {
            m_escapeState = EscapeState::Fading;
        }
    }

    // 透明化フェーズ
    if (m_escapeState == EscapeState::Fading && m_escapeAlpha > 0.0f) {
        m_escapeAlpha -= deltaSeconds * FADE_OUT_SPEED;
        if (m_escapeAlpha <= 0.0f) {
            m_escapeAlpha = 0.0f;
            m_escapeState = EscapeState::Done; // 完全に消失

            // 占有していたタイルを解放
            if (m_context && GetMap()) {
                Tile* t = GetMap()->GetTile(m_gridX, m_gridZ);
                if (t && t->occupant == this) t->occupant = nullptr;
            }
            // 吹き出しUIを閉じる
            if (m_context && m_context->GetDialogueUI()) {
                m_context->GetDialogueUI()->HideDialogue();
            }
        }
    }

    UpdateFacingRotation(deltaSeconds);
    if (m_isTurning) { UpdateWorldMatrix(); return; }


    // ノックバック中のスライディング更新（優先）
    if (m_isKnockedBack) {
        if (m_slideEndPos.LengthSquared() > 0.001f) {
            if (UpdateSlideAnimation(deltaSeconds)) {
                m_isKnockedBack = false;
                m_slideEndPos = Vector3(0, 0, 0);

                // 押し出された先のタイルにギミック（罠など）があるかチェック
                Tile* currentTile = GetMap()->GetTile(m_gridX, m_gridZ);
                if (currentTile && currentTile->structure) {
                    DBG_TRACE("[Ally] Knocked into an event/trap!");
                    currentTile->structure->OnEnter(this);
                }
            }
        }
        else {
            // 実際の移動が発生しなかった場合（例：即座に壁に衝突したなど）
            m_isKnockedBack = false;
        }

        UpdateWorldMatrix();
        return;
    }

    UpdateAlarmState(deltaSeconds);
    if (m_isDigging) { UpdateDiggingAnimation(deltaSeconds); }
    UpdateWorldMatrix();

}

void Ally::OnDraw(float /*deltaSeconds*/) {
    if (m_escapeAlpha <= 0.0f) return;

    if (m_shader) m_shader->SetGPU();
    Renderer::SetBlendState(BS_ALPHABLEND);
    Renderer::SetDepthEnable(true);
    Renderer::SetWorldMatrix(&m_worldMatrix);

    if (m_escapeState == EscapeState::Fading) {
        // アルファブレンド用のマテリアルオーバーライド
        auto OverrideAlpha = [this](CStaticMeshRenderer* renderer) {
            if (!renderer || !renderer->GetMaterial(0)) return;
            MATERIAL m = renderer->GetMaterial(0)->GetData();
            m.Diffuse = Color(1.0f, 1.0f, 1.0f, m_escapeAlpha);
            renderer->GetMaterial(0)->SetMaterial(m);
            };
        auto RestoreAlpha = [](CStaticMeshRenderer* renderer) {
            if (!renderer || !renderer->GetMaterial(0)) return;
            MATERIAL m = renderer->GetMaterial(0)->GetData();
            m.Diffuse = Color(1.0f, 1.0f, 1.0f, 1.0f);
            renderer->GetMaterial(0)->SetMaterial(m);
            };

        OverrideAlpha(m_renderer);
        DrawModel();
        RestoreAlpha(m_renderer);
    }
    else {
       // 通常状態はそのまま描画
        DrawModel();
    }

    Renderer::SetBlendState(BS_NONE);
}

void Ally::DrawUI() {
    // 脱出中/脱出後は無敵で退場するため、HPバーを表示しない
    if (IsEscaping() || IsEscapeDone()) return;
    Unit::DrawUI();
}

void Ally::TakeDamage(int damage, Unit* attacker) {
    Unit::TakeDamage(damage, attacker);
    if (m_currentHP <= 0 && !IsEscaping() && !m_isDeadFlying) {
        m_isDeadFlying = true;
        if (m_context && GetMap()) {
            Tile* myTile = GetMap()->GetTile(m_gridX, m_gridZ);
            if (myTile && myTile->occupant == this) myTile->occupant = nullptr;
        }
        StartDeathFly();
        if (m_context && m_context->GetCamera())
            m_context->GetCamera()->PlayKillCam(m_hitSourcePos, m_srt.pos,true);
    }
}

void Ally::OnDeathFlyComplete() {
    Destroy();
}

void Ally::StartTurn() {
    if (m_currentHP <= 0) return;
    // 脱出シーケンス進行中（採掘完了後）は通常採掘を行わない。Armed は「脱出採掘」なので許可
    if (m_escapeState != EscapeState::None && m_escapeState != EscapeState::Armed) return;
    m_isDigging = true;
    m_digTimer = 0.0f;
    m_digCount = 0;
    m_hasTriggeredEffect = false;
    m_srt.rot.z = 0.0f;

    // 開場（初回）のみ長めに掘って特写の見せ場を作る
    m_digTargetCount = m_hasPlayedIntroDig ? MAX_DIG_COUNT : INTRO_DIG_COUNT;
    m_hasPlayedIntroDig = true;
}

void Ally::OnTurnChanged(TurnState state) {
    if (state == TurnState::PlayerPhase) {
        StartTurn();
    }
}

void Ally::OnPushed(Direction pushDir, Unit* attacker) {
    if (IsEscaping()) return;

    m_isKnockedBack = true;
    Unit::OnPushed(pushDir);
}

void Ally::ArmEscape() {
    if (m_escapeState != EscapeState::None) return;
    m_escapeState = EscapeState::Armed;
    SetInvincible(true);   // 予約時点から無敵化（採掘～消失まで安全）
}

void Ally::UpdateDiggingAnimation(float dt) {
    m_digTimer += dt;

    // 正弦波によるツルハシの振り下ろしシミュレーション
    float angle = sinf(m_digTimer * DIG_SPEED) * DIG_SWING_AMPLITUDE;
    m_srt.rot.z = angle;

    // 振り下ろした瞬間のインパクト判定
    if (angle > DIG_HIT_ANGLE && !m_hasTriggeredEffect) {
        AudioManager::GetInstance().PlaySE("DigSE", DIG_SE_VOLUME);
        if (GetEffectManager()) {
            Vector3 footPos = m_srt.pos;
            footPos.x -= RUBBLE_OFFSET_X;
            GetEffectManager()->Spawn3DRubble(footPos);
        }
        m_hasTriggeredEffect = true;
        ++m_digCount;
    }

    // トリガーフラグのリセット（反対側に振り戻した際にリセット）
    if (angle < 0.0f) {
        m_hasTriggeredEffect = false;
    }

    // 終了判定
      // 終了判定
    if (m_digCount >= m_digTargetCount && angle < DIG_SETTLE_TOLERANCE && angle > -DIG_SETTLE_TOLERANCE) {
        m_isDigging = false;
        m_srt.rot.z = 0.0f;
        // 脱出予約中なら、採掘完了で脱出点出現フェーズへ
        if (m_escapeState == EscapeState::Armed) {
            m_escapeState = EscapeState::PointReveal;
            m_escapeSeqTimer = 0.0f;
        }
    }

    UpdateWorldMatrix();
}

void Ally::UpdateAlarmState(float dt) {
    // ロックオンされているかを毎フレーム判定（チャージ敵の死亡・自分の被押し出しで自動解除）
    bool targeted = false;
    if (!IsEscaping() && !IsEscapeDone() && m_currentHP > 0 && m_context && m_context->GetEnemyManager()) {
        targeted = m_context->GetEnemyManager()->IsAnyEnemyTargeting(m_gridX, m_gridZ);
    }

    if (targeted && !m_isAlarmed) {
        m_isAlarmed = true;
        m_alarmSignalStarted = false;   // 演出（ジャンプ＋吹き出し）は採掘完了後に開始
    }
    else if (!targeted && m_isAlarmed) {
        // 警告解除：接地へ戻す
        m_isAlarmed = false;
        if (m_alarmSignalStarted) {
            m_alarmSignalStarted = false;
            m_srt.pos.y = m_jumpBaseY;
        }
    }

    if (!m_isAlarmed) return;

    // 採掘優先：採掘が終わるまで警告演出を待機（採掘が割り込んだ場合は接地へ戻す）
    if (m_isDigging) {
        if (m_alarmSignalStarted) {
            m_alarmSignalStarted = false;
            m_srt.pos.y = m_jumpBaseY;
        }
        return;
    }

    // 採掘完了後の最初のフレームで警告演出を開始
    if (!m_alarmSignalStarted) {
        m_alarmSignalStarted = true;
        m_alarmTimer = 0.0f;
        m_jumpBaseY = m_srt.pos.y;
        if (m_context->GetDialogueUI()) {
            m_context->GetDialogueUI()->ShowDialogue(m_srt.pos, DialogueType::Danger, ALARM_DIALOGUE_DURATION);
        }
    }

    // ジャンプ3回 → 休止 → 繰り返し（|sin|の半周期 = ジャンプ1回）
    m_alarmTimer += dt;
    const float jumpPhaseLen = ALARM_JUMP_COUNT * PI / ALARM_JUMP_SPEED;
    const float cycleLen = jumpPhaseLen + ALARM_JUMP_PAUSE;
    float phase = fmodf(m_alarmTimer, cycleLen);
    m_srt.pos.y = (phase < jumpPhaseLen)
        ? m_jumpBaseY + fabsf(sinf(phase * ALARM_JUMP_SPEED)) * ALARM_JUMP_HEIGHT
        : m_jumpBaseY;
}
