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

namespace {
    // 演出・バランス用定数
    const int INITIAL_HP = 4;
    const int MAX_DIG_COUNT = 3;           // 採掘アニメーションの振り下ろし回数
    const float DIG_SPEED = 15.0f;         // 採掘アニメーションの速度
    const float DIG_HIT_ANGLE = 0.4f;      // 採掘エフェクトを発生させる閾値角度（ラジアン）
    const float FADE_OUT_SPEED = 1.0f;     // 脱出時のフェードアウト速度
    const float MODEL_SCALE = 0.8f;        // 味方モデルの表示スケール

    // 採掘演出の詳細パラメータ
    const float DIG_SWING_AMPLITUDE = 0.5f;    // ツルハシ振りの最大角度（ラジアン）
    const float DIG_SETTLE_TOLERANCE = 0.1f;   // 採掘終了とみなす静止角度の許容範囲
    const float DIG_SE_VOLUME = 2.6f;          // 採掘SEの音量
    const float RUBBLE_OFFSET_X = 0.5f;        // 瓦礫エフェクトの足元オフセット
    const int RUBBLE_SPAWN_COUNT = 3;          // 一振りで発生させる瓦礫の数
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

}

void Ally::Update(float deltaSeconds) {
    Unit::Update(deltaSeconds);

    if (m_isDeadFlying) {
        UpdateDeathFly(deltaSeconds);
        return;
    }

    // 出演出：透明化フェーズ
    if (m_escapeState == EscapeState::Fading && m_escapeAlpha > 0.0f) {
        m_escapeAlpha -= deltaSeconds * FADE_OUT_SPEED;
        if (m_escapeAlpha <= 0.0f) {
            m_escapeAlpha = 0.0f;
            m_escapeState = EscapeState::Done; // 全に消失

            // 占有していたタイルを解放
            if (m_context && GetMap()) {
                Tile* t = GetMap()->GetTile(m_gridX, m_gridZ);
                if (t && t->occupant == this) t->occupant = nullptr;
            }
            //  吹き出しUIを閉じる
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

    if (m_isDigging) { UpdateDiggingAnimation(deltaSeconds); }
    UpdateWorldMatrix();

}

void Ally::OnDraw(float /*deltaSeconds*/) {
    if (m_escapeAlpha <= 0.0f) return;

    if (m_shader) m_shader->SetGPU();
    Renderer::SetBlendState(BS_ALPHABLEND);
    Renderer::SetDepthEnable(true);
    Renderer::SetWorldMatrix(&m_worldMatrix);

    if (m_isEscaping) {
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

void Ally::TakeDamage(int damage, Unit* attacker) {
    Unit::TakeDamage(damage, attacker);
    if (m_currentHP <= 0 && !m_isEscaping && !m_isDeadFlying) {
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
    // ターン開始時、採掘アニメーション実行する
    DBG_ERROR("[Ally] Start Turn: Start Digging!");
    m_isDigging = true;
    m_digTimer = 0.0f;
    m_digCount = 0;
    m_hasTriggeredEffect = false;
    m_srt.rot.z = 0.0f;
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

void Ally::TriggerEscape() {
    if (m_isEscaping) return;
    m_isEscaping = true;
    SetInvincible(true);// 無敵化

    // 採掘を強制開始
    m_isDigging = true;
    m_digTimer = 0.0f;
    m_digCount = 0;
    m_hasTriggeredEffect = false;
    m_srt.rot.z = 0.0f;

    m_escapeState = EscapeState::Digging;

    // 脱出ダイアログを表示（ - 1.0fを渡して自動消失を無効化）
    if (m_context && m_context->GetDialogueUI()) {
        m_context->GetDialogueUI()->ShowDialogue(m_srt.pos, DialogueType::Escape, -1.0f);
    }
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
            GetEffectManager()->SpawnRubble(footPos, RUBBLE_SPAWN_COUNT);
        }
        m_hasTriggeredEffect = true;
        ++m_digCount;
    }

    // トリガーフラグのリセット（反対側に振り戻した際にリセット）
    if (angle < 0.0f) {
        m_hasTriggeredEffect = false;
    }

    // 終了判定
    if (m_digCount >= MAX_DIG_COUNT && angle < DIG_SETTLE_TOLERANCE && angle > -DIG_SETTLE_TOLERANCE) {
        m_isDigging = false;
        m_srt.rot.z = 0.0f;
        // ---脱出モードでの採掘完了後、フェードアウト状態へ遷移 ---
        if (m_escapeState == EscapeState::Digging) {
            m_escapeState = EscapeState::Fading;
        }
    }

    UpdateWorldMatrix();
}
