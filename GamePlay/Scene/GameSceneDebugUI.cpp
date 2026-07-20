#include "GameSceneDebugUI.h"
#include "GameScene.h"                              // friend アクセスに完全型が必要

#include "../../System/Renderer.h"
#include "../../System/Camera.h"
#include "../../System/DebugUI.h"                   // ImGui
#include "../../System/ZFightTunables.h"
#include "../../System/Utility/WorldToScreen.h"
#include "../../Core/Application.h"
#include "../../Core/GameContext.h"
#include "../../Types/Direction.h"
#include "../../GamePlay/Manager/MapManager.h"
#include "../../GamePlay/Manager/EnemyManager.h"
#include "../../GamePlay/Manager/TurnManager.h"
#include "../../GamePlay/Manager/EffectManager.h"
#include "../../UI/Component/HPBar.h"
#include "../../UI/Component/TurnCounter.h"
#include "../../UI/System/GameUIManager.h"
#include "../../Actor/Character/Player.h"
#include "../../Actor/Character/Ally.h"
#include "../../Actor/Character/Enemy.h"
#include "../../Actor/Base/Unit.h"
#include "../../System/FxTunables.h"
#include <stdio.h>

void GameSceneDebugUI::DrawCameraTuningWindow() {
    if (!m_enabled) return;

    // --- 読み取り専用の管理器ポインタを同名で別名化（本体を原型どおりに保つため） ---
    GameContext* m_context = m_scene.m_context;
    Camera* m_camera = m_scene.m_camera;
    Player* m_player = m_scene.m_player;
    Ally* m_ally = m_scene.m_ally;
    GameUIManager* m_gameUIManager = m_scene.m_gameUIManager;

    ImGui::Begin("Player Camera Tuning");
    if (ImGui::CollapsingHeader("Compare")) {
        TOONPARAM tp = Renderer::GetToonParam();
        bool changed = false;

        ImGui::Checkbox("Action UI", &m_scene.m_showActionUI);

        // アウトライン
        static bool outlineOn = true;
        static float savedW = tp.OutlineColor.w;
        if (ImGui::Checkbox("Outline", &outlineOn)) {
            if (!outlineOn) { savedW = tp.OutlineColor.w; tp.OutlineColor.w = 0.0f; }
            else { tp.OutlineColor.w = savedW; }
            changed = true;
        }

        // 半球光（OFF時：Sky を Ground に合わせ、単色 ambient へ退化）
        static bool hemiOn = true;
        static Color savedSky = tp.SkyColor;
        if (ImGui::Checkbox("Hemisphere", &hemiOn)) {
            if (!hemiOn) { savedSky = tp.SkyColor; tp.SkyColor = tp.ShadowColor; }
            else { tp.SkyColor = savedSky; }
            changed = true;
        }

        // 平行光によるトゥーン階調（OFF時：閾値を1以上へ移動し band を常に0にして環境光のみ表示）
        static bool lightOn = true;
        static Vector4 savedTP = tp.ToonParams;
        if (ImGui::Checkbox("Directional Light", &lightOn)) {
            if (!lightOn) { savedTP = tp.ToonParams; tp.ToonParams.x = 2.0f; tp.ToonParams.y = 2.0f; }
            else { tp.ToonParams = savedTP; }
            changed = true;
        }

        static bool shadowOn = true;
        if (ImGui::Checkbox("Shadow", &shadowOn)) {
            Renderer::s_shadowEnabled = shadowOn;
        }

        ImGui::Checkbox("HP Bar", &Unit::s_hpBarVisible);

        if (changed) Renderer::SetToonParam(tp);
    }

    if (ImGui::CollapsingHeader("Z-Fight Offsets")) {
        ImGui::SliderFloat("Range Panel", &ZFight::RangePanel, 0.0f, 0.5f, "%.3f");
        ImGui::SliderFloat("Arrow", &ZFight::Arrow, 0.0f, 0.5f, "%.3f");
        ImGui::SliderFloat("Path Line", &ZFight::PathLine, 0.0f, 0.5f, "%.3f");
        ImGui::SliderFloat("Ghost", &ZFight::Ghost, 0.0f, 0.5f, "%.3f");
        ImGui::SliderFloat("Enemy Arrow", &ZFight::EnemyArrow, 0.0f, 0.5f, "%.3f");
        ImGui::SliderFloat("Blob Shadow", &ZFight::Blob, 0.0f, 0.5f, "%.3f");
        ImGui::SliderFloat("Trap", &ZFight::Trap, 0.0f, 1.0f, "%.3f");
    }

    if (ImGui::CollapsingHeader("PostFX")) {
        POSTFX p = Renderer::GetPostFX();
        bool ch = false;
        ch |= ImGui::SliderFloat("Vignette", &p.Vignette, 0.0f, 2.0f, "%.2f");
        if (ch) Renderer::SetPostFX(p);
    }

    // 1. 現在プレイヤーターン（操作フェーズ）かをチェック
    bool isPlayerPhase = false;
    if (m_context && m_context->GetTurnManager()) {
        isPlayerPhase = (m_context->GetTurnManager()->GetTurnState() == TurnState::PlayerPhase);
    }

    // プレイヤーターンでない場合は、赤字の警告を表示して操作をロック
    if (!isPlayerPhase) {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Tuning only available in Player Phase.");
        ImGui::End();
        return;
    }

    // 2. プレイヤーターン中：実際のカメラパラメータを読み取ってバインド
    ImGui::Text("Action Camera Parameters");
    ImGui::Separator();

    bool changed = false;

    // Camera クラスのグローバル inline 変数に直接バインド
    if (ImGui::SliderFloat("Zoom Radius", &Camera::ZOOM_RADIUS, 10.0f, 50.0f, "%.1f")) changed = true;
    if (ImGui::SliderFloat("Azimuth", &Camera::BASE_AZIMUTH, -3.14159f, 3.14159f, "%.4f")) changed = true;
    if (ImGui::SliderFloat("Elevation", &Camera::BASE_ELEVATION, -1.5f, 1.5f, "%.4f")) changed = true;

    // 3. パラメータ変更時、新しい値をカメラの【目標値 (Target)】へ設定
    if (changed && m_camera) {
        m_camera->SetTargetRadius(Camera::ZOOM_RADIUS);
        m_camera->UpdateTargetAzimuth();
        m_camera->SetTargetElevation(Camera::BASE_ELEVATION);
    }

    if (ImGui::CollapsingHeader("Outline")) {
        TOONPARAM tp = Renderer::GetToonParam();
        bool ch = false;
        ch |= ImGui::SliderFloat("Width", &tp.OutlineColor.w, 0.0f, 0.001f, "%.4f");
        float col[3] = { tp.OutlineColor.x, tp.OutlineColor.y, tp.OutlineColor.z };
        if (ImGui::ColorEdit3("Color", col)) {
            tp.OutlineColor.x = col[0]; tp.OutlineColor.y = col[1]; tp.OutlineColor.z = col[2];
            ch = true;
        }
        if (ch) Renderer::SetToonParam(tp);
    }

    ImGui::Separator();

    // 境界（バウンディングボックス）のパディング設定
    if (ImGui::SliderFloat("Bound Padding", &Camera::BOUND_PADDING, -10.0f, 10.0f, "%.1f")) {
        m_scene.RecalculateCameraBounds();
    }
    // === デバッグ用：ライティング調整 ===
    if (ImGui::CollapsingHeader("Light / Toon")) {
        LIGHT lt = Renderer::GetLight();
        float dir[3] = { lt.Direction.x, lt.Direction.y, lt.Direction.z };
        float intensity = lt.Diffuse.x;
        float ambient = lt.Ambient.x;

        bool ch = false;
        ch |= ImGui::SliderFloat3("Light Dir", dir, -1.0f, 1.0f);
        ch |= ImGui::SliderFloat("Light Intensity", &intensity, 0.0f, 3.0f);

        if (ch) {
            Vector4 d(dir[0], dir[1], dir[2], 0.0f);
            d.Normalize();
            lt.Direction = d;
            lt.Diffuse = Color(intensity, intensity, intensity, 1.0f);
            lt.Ambient = Color(ambient, ambient, ambient, 1.0f);
            Renderer::SetLight(lt);
        }
    }

    if (ImGui::CollapsingHeader("Hemisphere")) {
        TOONPARAM tp = Renderer::GetToonParam();
        bool ch = false;
        float sky[3] = { tp.SkyColor.x,    tp.SkyColor.y,    tp.SkyColor.z };
        float ground[3] = { tp.ShadowColor.x, tp.ShadowColor.y, tp.ShadowColor.z };
        if (ImGui::ColorEdit3("Sky (up)", sky)) { tp.SkyColor.x = sky[0];      tp.SkyColor.y = sky[1];      tp.SkyColor.z = sky[2];      ch = true; }
        if (ImGui::ColorEdit3("Ground (dn)", ground)) { tp.ShadowColor.x = ground[0]; tp.ShadowColor.y = ground[1]; tp.ShadowColor.z = ground[2]; ch = true; }
        if (ch) Renderer::SetToonParam(tp);
    }

    // === デバッグ用：勝利アニメーションと「WIN」テキストの強制発火 ===
    ImGui::Text("Debug Actions");
    if (ImGui::Button("Test Win Animation & Text", ImVec2(-1, 30))) {
        if (m_player) {
            m_player->StartCelebration();
            if (m_camera) {
                m_camera->ChangeState(CameraState::TargetFocus, m_player->GetSRT().pos);
            }
        }
    }

    // === デバッグ用：脱出出現と指示画像の強制発火 ===
    ImGui::Text("Debug Escape");
    if (ImGui::Button("Test Escape Model & Text", ImVec2(-1, 30))) {
        m_scene.m_shouldShowDebugEscape = !m_scene.m_shouldShowDebugEscape;
        m_scene.m_turnCounter->SetTurn(0);
        if (m_scene.m_shouldShowDebugEscape && m_ally) {
            m_scene.m_escapeGridX = m_ally->GetUnitGridX();
            m_scene.m_escapeGridZ = m_ally->GetUnitGridZ();
        }
    }

    ImGui::Spacing();
    if (ImGui::CollapsingHeader("HP Bar")) {
        ImGui::SliderFloat("HP Y Offset", &HPBar::s_hpBarOffsetY, 0.0f, 4.0f, "%.2f");
        ImGui::SliderFloat("HP Heart Size", &HPBar::s_hpBarTexSize, 10.0f, 60.0f, "%.1f");
        ImGui::SliderFloat("HP Heart Gap", &HPBar::s_hpBarGap, 0.0f, 15.0f, "%.1f");
        if (m_gameUIManager) {
            ImGui::Spacing();
            ImGui::Text("Camera Rotate UI Tuning");
            ImGui::Separator();
            ImGui::SliderFloat("Rotate UI X", &m_gameUIManager->GetCameraRotatePos().x, 0.0f, 1920.0f, "%.1f");
            ImGui::SliderFloat("Rotate UI Y", &m_gameUIManager->GetCameraRotatePos().y, 0.0f, 1080.0f, "%.1f");
            ImGui::SliderFloat("Rotate UI Scale", &m_gameUIManager->GetCameraRotateScale(), 0.1f, 3.0f, "%.2f");
        }
    }
    ImGui::Spacing();
    // 設定をローカルの INI ファイルへ一括保存
    if (ImGui::Button("Save Config to Local", ImVec2(-1, 30))) {
        Camera::SaveConfig();
    }

    // ===== アタックカメラ =====
    ImGui::Spacing();
    if (ImGui::CollapsingHeader("Attack Cam")) {
        ImGui::SliderFloat("AttackZoom Radius", &Camera::ATTACKZOOM_RADIUS, 5.0f, 40.0f, "%.1f");
        ImGui::SliderFloat("AttackZoom Hold", &Camera::ATTACKZOOM_HOLD, 0.1f, 3.0f, "%.2f");
        ImGui::SliderFloat("Attack Lead", &Camera::ATTACK_ZOOM_LEAD, 0.0f, 1.0f, "%.2f");
        if (ImGui::Button("Test Attack Cam", ImVec2(-1, 30))) {
            SpawnDebugEnemyInFront(-1);
            m_player->DebugForceAttack(m_player->GetFacing());
        }
    }

    // ===== キルカメラ =====
    ImGui::Spacing();
    if (ImGui::CollapsingHeader("Kill Cam")) {
        ImGui::SliderFloat("KillCam Radius", &Camera::KILLCAM_RADIUS, 5.0f, 40.0f, "%.1f");
        ImGui::SliderFloat("KillCam Elevation", &Camera::KILLCAM_ELEVATION, -1.5f, 0.0f, "%.3f");
        ImGui::SliderFloat("Shoulder Yaw", &Camera::KILLCAM_SHOULDER_YAW, -1.5f, 1.5f, "%.3f");       
        ImGui::SliderFloat("Windup (Lead)", &Camera::KILLCAM_LEAD, 0.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("Wait Timeout", &Camera::KILLCAM_WAIT_TIMEOUT, 0.5f, 4.0f, "%.1f");
        ImGui::SliderFloat("KillCam Hold (slow)", &Camera::KILLCAM_HOLD, 0.2f, 3.0f, "%.2f");
        ImGui::SliderFloat("Time Scale", &Camera::KILLCAM_TIME_SCALE, 0.05f, 1.0f, "%.2f");
        ImGui::SliderFloat("Pitch Lift (look up)", &Camera::KILLCAM_PITCH_LIFT, 0.0f, 30.0f, "%.1f");
        ImGui::SliderFloat("Follow Weight", &Camera::KILLCAM_FOLLOW_WEIGHT, 0.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("Follow Max Y", &Camera::KILLCAM_FOLLOW_MAX_Y, 0.0f, 15.0f, "%.1f");
        ImGui::SliderFloat("Follow Min Y", &Camera::KILLCAM_FOLLOW_MIN_Y, -5.0f, 5.0f, "%.1f");
        ImGui::SliderFloat("Return Speed", &Camera::CINE_RETURN_LERP_SPEED, 0.5f, 8.0f, "%.1f");
        ImGui::SliderFloat("Lerp Speed", &Camera::CAMERA_LERP_SPEED, 0.5f, 12.0f, "%.1f");
        ImGui::SliderFloat("Pan Max Dist", &Camera::KILLCAM_PAN_MAX_DIST, 0.0f, 6.0f, "%.1f");
        if (ImGui::Button("Test Kill Cam", ImVec2(-1, 30))) {
            SpawnDebugEnemyInFront(1);
            m_player->DebugForceAttack(m_player->GetFacing());
        }
    }

    if (ImGui::CollapsingHeader("FX")) {
        // --- 手応えパラメータ ---
        ImGui::SliderInt("Burst Count", &Fx::Burst.count, 5, 60);
        ImGui::SliderFloat("Burst Speed Max", &Fx::Burst.speedMax, 2.0f, 15.0f);
        ImGui::SliderFloat("Shake Duration", &Fx::DeathShake.duration, 0.1f, 1.0f);
        ImGui::SliderFloat("Star Arm Len", &Fx::Star.armLen, 0.5f, 3.0f);
        ImGui::SliderFloat("Star Progress", &Fx::Star.progress, 0.3f, 0.9f);
        ImGui::SliderFloat("Rubble Bounce", &Fx::Rubble.restitution, 0.0f, 0.9f);

        // --- 各エフェクトの色（Color は XMFLOAT4 派生のため &c.x を直接バインド） ---
        if (ImGui::TreeNode("Colors")) {
            ImGui::ColorEdit4("Rubble 1", &Fx::Rubble.colors[0].x);
            ImGui::ColorEdit4("Rubble 2", &Fx::Rubble.colors[1].x);
            ImGui::ColorEdit4("Rubble 3", &Fx::Rubble.colors[2].x);
            ImGui::Separator();
            ImGui::ColorEdit4("Hit 1", &Fx::Hit.colors[0].x);
            ImGui::ColorEdit4("Hit 2", &Fx::Hit.colors[1].x);
            ImGui::Separator();
            ImGui::ColorEdit4("Burst 1", &Fx::Burst.colors[0].x);
            ImGui::ColorEdit4("Burst 2", &Fx::Burst.colors[1].x);
            ImGui::ColorEdit4("Burst 3", &Fx::Burst.colors[2].x);
            ImGui::ColorEdit4("Burst 4", &Fx::Burst.colors[3].x);
            ImGui::Separator();
            ImGui::ColorEdit4("Trail", &Fx::Trail.color.x);
            ImGui::ColorEdit4("Star", &Fx::Star.color.x);
            ImGui::TreePop();
        }

        bool additive = Fx::Render.additive != 0;
        if (ImGui::Checkbox("Additive Blend", &additive)) Fx::Render.additive = additive ? 1 : 0;

        // --- INI 永続化（Camera と同じ現場調整→保存の流れ） ---
        if (ImGui::Button("Save##fx")) Fx::SaveConfig();
        ImGui::SameLine();
        if (ImGui::Button("Load##fx")) Fx::LoadConfig();
    }

    // --- 各エフェクトのサイズ調整 ---
    if (ImGui::CollapsingHeader("FX Size")) {
        ImGui::SliderFloat("Rubble Scale Min", &Fx::Rubble.scaleMin, 0.02f, 0.5f);
        ImGui::SliderFloat("Rubble Scale Max", &Fx::Rubble.scaleMax, 0.02f, 0.6f);
        ImGui::SliderFloat3("Hit Spark Scale", &Fx::Hit.sparkScale.x, 0.02f, 0.8f);
        ImGui::SliderFloat("Burst Scale Min", &Fx::Burst.scaleMin, 0.02f, 0.4f);
        ImGui::SliderFloat("Burst Scale Max", &Fx::Burst.scaleMax, 0.02f, 0.5f);
        ImGui::SliderFloat("Trail Scale", &Fx::Trail.scale, 0.02f, 0.5f);
        ImGui::SliderFloat("Star Arm Thick", &Fx::Star.armThick, 0.05f, 0.6f);

        // --- 即時プレビュー（プレイヤー位置で再生、パラメータ調整の確認用） ---
        if (m_scene.m_player && m_scene.m_context && m_scene.m_context->GetEffectManager()) {
            EffectManager* fx = m_scene.m_context->GetEffectManager();
            const Vector3 base = m_scene.m_player->GetSRT().pos;
            const float PREVIEW_HIT_Y = 0.6f;   // 打撃：体の中心あたり
            const float PREVIEW_STAR_Y = 1.2f;  // スター：頭上
            const float PREVIEW_TRAIL_STEP = 0.35f; // トレイル：縦に並べて軌跡を模擬

            if (ImGui::Button("Play Rubble")) fx->Spawn3DRubble(base);
            ImGui::SameLine();
            if (ImGui::Button("Play Hit")) {
                Vector3 p = base; p.y += PREVIEW_HIT_Y;
                fx->Spawn3DHit(p);
            }
            ImGui::SameLine();
            if (ImGui::Button("Play Burst")) {
                Vector3 p = base; p.y += Fx::Burst.spawnYOffset;
                fx->Spawn3DDeathBurst(p);
            }
            if (ImGui::Button("Play Star")) {
                Vector3 p = base; p.y += PREVIEW_STAR_Y;
                fx->Spawn3DStarCross(p);
            }
            ImGui::SameLine();
            if (ImGui::Button("Play Trail x10")) {
                // 実際は飛翔軌跡に沿って生成されるため、縦一列で雰囲気を確認する
                for (int i = 0; i < 10; ++i) {
                    Vector3 p = base;
                    p.y += PREVIEW_TRAIL_STEP * i;
                    fx->Spawn3DTrailPuff(p);
                }
            }
        }
    }
    ImGui::End();
}

Enemy* GameSceneDebugUI::SpawnDebugEnemyInFront(int hp) {
    Player* m_player = m_scene.m_player;
    GameContext* m_context = m_scene.m_context;
    MapManager* m_mapManager = m_scene.m_mapManager;

    if (!m_player || !m_context || !m_mapManager) return nullptr;

    // 前回テストで残ったデバッグ敵を掃除し、スタックを防ぐ
    if (m_debugEnemy) {
        if (m_context->GetEnemyManager())
            m_context->GetEnemyManager()->RemoveEnemy(m_debugEnemy);
        Tile* prev = m_mapManager->GetTile(
            m_debugEnemy->GetUnitGridX(), m_debugEnemy->GetUnitGridZ());
        if (prev && prev->occupant == m_debugEnemy) prev->occupant = nullptr;
        m_debugEnemy->Destroy();
        m_debugEnemy = nullptr;
    }

    DirOffset o = DirOffset::From(m_player->GetFacing());
    int gx = m_player->GetUnitGridX() + o.x;
    int gz = m_player->GetUnitGridZ() + o.z;
    Tile* t = m_mapManager->GetTile(gx, gz);
    if (!t) return nullptr;

    auto enemy = Enemy::Spawn(m_context, gx, gz, m_mapManager->GetWorldPosition(gx, gz));
    if (hp > 0) enemy->DebugSetHP(hp);

    t->occupant = enemy.get();
    Enemy* raw = enemy.get();
    if (m_context->GetEnemyManager()) m_context->GetEnemyManager()->RegisterEnemy(raw);
    m_scene.AddObject(std::move(enemy));
    m_debugEnemy = raw;
    return raw;
}