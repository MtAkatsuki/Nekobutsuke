#include "Ally.h"
#include "../manager/GameContext.h"
#include "../manager/MapManager.h"
#include "../system/meshmanager.h"
#include "../utility/WorldToScreen.h"
#include "../Application.h"
#include "../ui/DialogueUI.h"
#include "../manager/AudioManager.h"
#include "../manager/EffectManager.h"

void Ally::Init() 
{   //モデル関連のソースをロード　
        // ---  Model Front ---
    {
        std::unique_ptr<CStaticMesh> mesh = std::make_unique<CStaticMesh>();
        mesh->Load("assets/model/character/ally_front.obj", "assets/model/character/");
        std::unique_ptr<CStaticMeshRenderer> renderer = std::make_unique<CStaticMeshRenderer>();
        renderer->Init(*mesh);
        MeshManager::RegisterMesh<CStaticMesh>("ally_front_mesh", std::move(mesh));
        MeshManager::RegisterMeshRenderer<CStaticMeshRenderer>("ally_front_mesh", std::move(renderer));
    }
    // ---  Model Back ---
    {
        std::unique_ptr<CStaticMesh> mesh = std::make_unique<CStaticMesh>();
        mesh->Load("assets/model/character/ally_back.obj", "assets/model/character/");
        std::unique_ptr<CStaticMeshRenderer> renderer = std::make_unique<CStaticMeshRenderer>();
        renderer->Init(*mesh);
        MeshManager::RegisterMesh<CStaticMesh>("ally_back_mesh", std::move(mesh));
        MeshManager::RegisterMeshRenderer<CStaticMeshRenderer>("ally_back_mesh", std::move(renderer));
    }


    m_shader = MeshManager::getShader<CShader>("unlightshader");

    auto* frontR = MeshManager::getRenderer<CStaticMeshRenderer>("ally_front_mesh");
    auto* backR = MeshManager::getRenderer<CStaticMeshRenderer>("ally_back_mesh");
    SetModelRenderers(frontR, backR);


    //初期ステータスを設置
    m_maxHP = 4;
    m_currentHP = m_maxHP;
    m_team = Team::Ally;
    m_maxMovePoints = 0;//現在仲間は移動できない
    m_currentMovePoints = m_maxMovePoints;

    m_srt.scale = Vector3(-1.0f, 1.0f, 1.0f);
    SetFacing(Direction::South);
    UpdateWorldMatrix();

}

void Ally::TakeDamage(int damage, Unit* attacker) {
    Unit::TakeDamage(damage, attacker);
}

void Ally::Update(uint64_t deltatime)
{
    float dt = static_cast<float>(deltatime) / 1000.0f;

	// 脱出中は徐々に透明になる
    if (m_escapeState == EscapeState::Fading && m_escapeAlpha > 0.0f) {
        m_escapeAlpha -= dt * 1.0f;
        if (m_escapeAlpha <= 0.0f) {
            m_escapeAlpha = 0.0f;
            m_escapeState = EscapeState::Done; // 完全に消失

            // 占有していたタイルを解放
            if (m_context && m_context->GetMapManager()) {
                Tile* t = m_context->GetMapManager()->GetTile(m_gridX, m_gridZ);
                if (t && t->occupant == this) {
                    t->occupant = nullptr;
                }
            }
            // 吹き出しUIを閉じる
            if (m_context && m_context->GetDialogueUI()) {
                m_context->GetDialogueUI()->HideDialogue();
            }
        }
    }

    UpdateFlipAnimation(dt);
	// フリップ中は他のアニメーションを更新しない
    if (m_isFlipping){UpdateWorldMatrix();return;}

    if (m_isDigging) {UpdateDiggingAnimation(dt);}
    UpdateWorldMatrix();
}

void Ally::OnDraw(uint64_t deltatime)
{   
	if (m_escapeAlpha <= 0.0f) return; //完全に透明なら描画しない
    //Allyの描画関数
    if (m_shader) m_shader->SetGPU();

        Renderer::SetPixelArtMode(true);
        Renderer::SetBlendState(BS_ALPHABLEND);
        Renderer::SetDepthEnable(true);

        Renderer::SetWorldMatrix(&m_WorldMatrix);
        // マテリアルのAlpha値を動的に変更
        if (m_isEscaping) {
            // --- Front マテリアルの取得と変更 ---
            CMaterial* frontMtrl = nullptr;
            MATERIAL oldFront;
            if (m_frontRenderer && m_frontRenderer->GetMaterial(0)) {
                frontMtrl = m_frontRenderer->GetMaterial(0);
                oldFront = frontMtrl->GetData();
                MATERIAL alphaMat = oldFront;
                alphaMat.Diffuse = Color(1.0f, 1.0f, 1.0f, m_escapeAlpha); // 動的なAlpha値
                frontMtrl->SetMaterial(alphaMat);
            }

            // --- Back マテリアルの取得と変更 ---
            CMaterial* backMtrl = nullptr;
            MATERIAL oldBack;
            if (m_backRenderer && m_backRenderer->GetMaterial(0)) {
                backMtrl = m_backRenderer->GetMaterial(0);
                oldBack = backMtrl->GetData();
                MATERIAL alphaMat = oldBack;
                alphaMat.Diffuse = Color(1.0f, 1.0f, 1.0f, m_escapeAlpha); // 動的なAlpha値
                backMtrl->SetMaterial(alphaMat);
            }

            // 現在の向きに応じたモデルを描画
            DrawModel();

            // --- Front マテリアルの復元 ---
            if (frontMtrl) {
                MATERIAL restore = oldFront;
                restore.Diffuse = Color(1.0f, 1.0f, 1.0f, 1.0f);
                restore.Ambient = Color(1.0f, 1.0f, 1.0f, 1.0f);
                restore.Emission = Color(0.1f, 0.1f, 0.1f, 1.0f);
                restore.TextureEnable = TRUE;
                frontMtrl->SetMaterial(restore);
            }

            // --- Back マテリアルの復元 ---
            if (backMtrl) {
                MATERIAL restore = oldBack;
                restore.Diffuse = Color(1.0f, 1.0f, 1.0f, 1.0f);
                restore.Ambient = Color(1.0f, 1.0f, 1.0f, 1.0f);
                restore.Emission = Color(0.1f, 0.1f, 0.1f, 1.0f);
                restore.TextureEnable = TRUE;
                backMtrl->SetMaterial(restore);
            }
        }
        else {
            // 通常状態はそのまま描画
            DrawModel();
        }

        Renderer::SetBlendState(BS_NONE);
        Renderer::SetPixelArtMode(false);
}

void Ally::StartTurn() {
    if (m_currentHP <= 0) return;
	// ターン開始時、採掘アニメーション実行する
    std::cout << "[Ally] Start Turn: Start Digging!" << std::endl;
    m_isDigging = true;
    m_digTimer = 0.0f;
    m_digCount = 0;
    m_hasTriggeredEffect = false;
	// リセット姿勢
    m_srt.rot.z = 0.0f;
}
// TurnStateの変更通知を受け取る
void Ally::OnTurnChanged(TurnState state)
{

    if (state == TurnState::PlayerPhase)
    {
        std::cout << "[Ally] It's PlayerPhase! Triggering StartTurn." << std::endl;
        StartTurn();
    }
}

void Ally::UpdateDiggingAnimation(float dt) {
    m_digTimer += dt;

    // 正弦波による揺れのシミュレーション: 0 -> 正 -> 0 -> 負 -> 0
    // 特定の極値点（最大値または最小値）を「地面を叩いた瞬間」として扱う
    // 仕様：負の角度で振り上げ、正の角度で振り下ろす
    float angle = sinf(m_digTimer * DIG_SPEED) * 0.5f; // +/- 0.5ラジアン (約30度)

    // Z軸（左右）またはX軸（前後）の回転。モデルの向きに合わせて調整。
    m_srt.rot.z = angle;

    // 「ヒット（叩いた瞬間）」の判定：
    // 簡易ロジック：角度が 0.4 を超えた（最大傾斜に近い）かつ本周期で未処理の場合
    if (angle > 0.4f && !m_hasTriggeredEffect) {
        std::cout << "[Ally] DEBUG: Triggering Dig Effect!" << std::endl;
        // 効果音（SE）の再生
        AudioManager::GetInstance().PlaySE("DigSE", 2.6f);

        // エフェクト（足元）の発生
        if (m_context->GetEffectManager()) {
            Vector3 footPos = m_srt.pos;
            // ツルハシの着弾点に合わせてオフセットを調整
            footPos.x -= 0.5f;
            m_context->GetEffectManager()->SpawnRubble(footPos, 3);
        }

        m_hasTriggeredEffect = true; // 本周期のフラグを立てる
        m_digCount++; // 採掘カウントをインクリメント
    }

    // トリガーフラグのリセット（反対側に振り戻した際にリセット）
    if (angle < 0.0f) {
        m_hasTriggeredEffect = false;
    }

    // 終了判定
    if (m_digCount >= MAX_DIG_COUNT && angle < 0.1f && angle > -0.1f) {
        // 3回掘り終わり、かつモデルがほぼ中央に戻ったタイミングで停止
        m_isDigging = false;
        m_srt.rot.z = 0.0f; // 姿勢のリセット
        // ---脱出モードでの採掘完了後、フェードアウト状態へ遷移 ---
        if (m_escapeState == EscapeState::Digging) {
            m_escapeState = EscapeState::Fading;
        }
    }

    UpdateWorldMatrix();
}
// 仲間が脱出する際の処理。敵からの攻撃を防止し、占有タイルを解放する。
void Ally::TriggerEscape() {
    if (m_isEscaping) return;
    m_isEscaping = true;
    m_currentHP = 0; // 敵からの攻撃を防止する

    // 採掘を強制開始
    m_isDigging = true;
    m_digTimer = 0.0f;
    m_digCount = 0;
    m_hasTriggeredEffect = false;
    m_srt.rot.z = 0.0f;

    m_escapeState = EscapeState::Digging;

    // 脱出ダイアログを表示（-1.0fを渡して自動消失を無効化）
    if (m_context && m_context->GetDialogueUI()) {
        m_context->GetDialogueUI()->ShowDialogue(m_srt.pos, DialogueType::Escape, -1.0f);
    }
}