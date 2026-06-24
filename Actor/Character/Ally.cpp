#include "Ally.h"
#include "../../Core/GameContext.h"
#include "../../GamePlay/Manager/MapManager.h"
#include "../../System/Meshmanager.h"
#include "../../System/Utility/WorldToScreen.h"
#include "../../Core/Application.h"
#include "../../UI/Component/DialogueUI.h"
#include "../../System/Audio/AudioManager.h"
#include "../../GamePlay/Manager/EffectManager.h"

namespace {
    // 演出・バランス用定数
    const int INITIAL_HP = 4;
    const int MAX_DIG_COUNT = 3;           // 採掘アニメーションの振り下ろし回数
    const float DIG_SPEED = 15.0f;         // 採掘アニメーションの速度
    const float DIG_HIT_ANGLE = 0.4f;      // 採掘エフェクトを発生させる閾値角度（ラジアン）
    const float FADE_OUT_SPEED = 1.0f;     // 脱出時のフェードアウト速度
}

void Ally::Init() 
{
    //モデル関連のソースをロード

    // ---  Model Front ---
    {
        std::unique_ptr<CStaticMesh> mesh = std::make_unique<CStaticMesh>();
        mesh->Load("Assets/model/character/Mouse/Mouse_01.obj", "Assets/model/character/Mouse");
        std::unique_ptr<CStaticMeshRenderer> renderer = std::make_unique<CStaticMeshRenderer>();
        renderer->Init(*mesh);
        MeshManager::RegisterMesh<CStaticMesh>("ally_mesh", std::move(mesh));
        MeshManager::RegisterMeshRenderer<CStaticMeshRenderer>("ally_mesh", std::move(renderer));
    }


    m_shader = MeshManager::getShader<CShader>("unlightshader");
    auto* renderer = MeshManager::getRenderer<CStaticMeshRenderer>("ally_mesh");
    SetModelRenderer(renderer);


    //初期ステータスを設置
    m_maxHP = INITIAL_HP;
    m_currentHP = m_maxHP;
    m_team = Team::Ally;
    m_maxMovePoints = 0;// 味方は自立移動しない
    m_currentMovePoints = m_maxMovePoints;
    m_srt.scale = Vector3(1.0f, 1.0f, 1.0f);
    SetFacing(Direction::South);
    UpdateWorldMatrix();

}

void Ally::Update(uint64_t deltatime) {
    Unit::Update(deltatime);
    float dt = static_cast<float>(deltatime) / 1000.0f;

    if (m_isDeadFlying) {
        UpdateDeathFly(dt);
        return;
    }

    // 出演出：透明化フェーズ
    if (m_escapeState == EscapeState::Fading && m_escapeAlpha > 0.0f) {
        m_escapeAlpha -= dt * FADE_OUT_SPEED;
        if (m_escapeAlpha <= 0.0f) {
            m_escapeAlpha = 0.0f;
            m_escapeState = EscapeState::Done; // 全に消失

            // 占有していたタイルを解放
            if (m_context && m_context->GetMapManager()) {
                Tile* t = m_context->GetMapManager()->GetTile(m_gridX, m_gridZ);
                if (t && t->occupant == this) t->occupant = nullptr;
            }
            //  吹き出しUIを閉じる
            if (m_context && m_context->GetDialogueUI()) {
                m_context->GetDialogueUI()->HideDialogue();
            }
        }
    }

    UpdateFacingRotation(dt);
    if (m_isTurning) { UpdateWorldMatrix(); return; }


    // ノックバック中のスライディング更新（優先）
    if (m_isKnockedBack) {
        if (m_slideEndPos.LengthSquared() > 0.001f) {
            if (UpdateSlideAnimation(deltatime)) {
                m_isKnockedBack = false;
                m_slideEndPos = Vector3(0, 0, 0);

                // 押し出された先のタイルにギミック（罠など）があるかチェック
                Tile* currentTile = m_context->GetMapManager()->GetTile(m_gridX, m_gridZ);
                if (currentTile && currentTile->structure) {
                    std::cout << "[Ally] Knocked into an event/trap!" << std::endl;
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

    if (m_isDigging) { UpdateDiggingAnimation(dt); }
    UpdateWorldMatrix();

}

void Ally::OnDraw(uint64_t deltatime) {
    if (m_escapeAlpha <= 0.0f) return;

    if (m_shader) m_shader->SetGPU();
    Renderer::SetBlendState(BS_ALPHABLEND);
    Renderer::SetDepthEnable(true);
    Renderer::SetWorldMatrix(&m_WorldMatrix);

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

        OverrideAlpha(m_Renderer);
        DrawModel();
        RestoreAlpha(m_Renderer);
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
        if (m_context && m_context->GetMapManager()) {
            Tile* myTile = m_context->GetMapManager()->GetTile(m_gridX, m_gridZ);
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
    std::cout << "[Ally] Start Turn: Start Digging!" << std::endl;
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

// 仲間が脱出する際の処理。敵からの攻撃を防止し、占有タイルを解放する。
void Ally::TriggerEscape() {
    if (m_isEscaping) return;
    m_isEscaping = true;
    m_currentHP = 0; // 無敵化

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
    float angle = sinf(m_digTimer * DIG_SPEED) * 0.5f;
    m_srt.rot.z = angle;

    // 振り下ろした瞬間のインパクト判定
    if (angle > DIG_HIT_ANGLE && !m_hasTriggeredEffect) {
        AudioManager::GetInstance().PlaySE("DigSE", 2.6f);
        if (m_context->GetEffectManager()) {
            Vector3 footPos = m_srt.pos;
            footPos.x -= 0.5f;
            m_context->GetEffectManager()->SpawnRubble(footPos, 3);
        }
        m_hasTriggeredEffect = true;
        m_digCount++;
    }

    // トリガーフラグのリセット（反対側に振り戻した際にリセット）
    if (angle < 0.0f) {
        m_hasTriggeredEffect = false;
    }

    // 終了判定
    if (m_digCount >= MAX_DIG_COUNT && angle < 0.1f && angle > -0.1f) {
        m_isDigging = false;
        m_srt.rot.z = 0.0f;
        // ---脱出モードでの採掘完了後、フェードアウト状態へ遷移 ---
        if (m_escapeState == EscapeState::Digging) {
            m_escapeState = EscapeState::Fading;
        }
    }

    UpdateWorldMatrix();
}
