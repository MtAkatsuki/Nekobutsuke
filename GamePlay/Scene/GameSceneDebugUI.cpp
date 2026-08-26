#include "GameSceneDebugUI.h"
#include "GameScene.h"                              // friend アクセスに完全型が必要

#include "../../System/Renderer.h"
#include "../../System/Camera.h"
#include "../../System/DebugUI.h"                   // ImGui
#include "../../System/CDirectInput.h"              // パネル用ホットキー
#include "../../System/ZFightTunables.h"
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

// =========================================================
// 入口 & ホットキー
// =========================================================
void GameSceneDebugUI::Draw() {
    HandlePanelHotkeys();

    // 総覧ウィンドウ：どのパネルがどのキーかを常に把握できるように
    ImGui::Begin("Debug Panels  [F4 master]");
    ImGui::TextUnformatted("Toggle panels:");
    ImGui::Checkbox("F5  View Mode / Camera", &m_showViewMode);
    ImGui::Checkbox("F6  Action Cam", &m_showActionCam);
    ImGui::Checkbox("F7  Rendering", &m_showRendering);
    ImGui::Checkbox("F8  FX", &m_showFx);
    ImGui::Checkbox("F9  Debug Actions", &m_showDebugActions);
    ImGui::Separator();
    ImGui::TextDisabled("F2: toggle Player <-> Strategy");
    ImGui::End();

    if (m_showViewMode)     DrawViewModePanel();
    if (m_showActionCam)    DrawActionCamPanel();
    if (m_showRendering)    DrawRenderingPanel();
    if (m_showFx)           DrawFxPanel();
    if (m_showDebugActions) DrawDebugActionsPanel();
}

void GameSceneDebugUI::HandlePanelHotkeys() {
    auto& in = CDirectInput::GetInstance();
    if (in.CheckKeyBufferTrigger(DIK_F5)) m_showViewMode = !m_showViewMode;
    if (in.CheckKeyBufferTrigger(DIK_F6)) m_showActionCam = !m_showActionCam;
    if (in.CheckKeyBufferTrigger(DIK_F7)) m_showRendering = !m_showRendering;
    if (in.CheckKeyBufferTrigger(DIK_F8)) m_showFx = !m_showFx;
    if (in.CheckKeyBufferTrigger(DIK_F9)) m_showDebugActions = !m_showDebugActions;
}

// =========================================================
// F5：View Mode & Camera
// =========================================================
void GameSceneDebugUI::DrawViewModePanel() {
    Camera* cam = m_scene.m_camera;
    Player* player = m_scene.m_player;

    ImGui::Begin("View Mode & Camera (F5)");

    // -- ObbitTest --
    {
        auto& in = CDirectInput::GetInstance();
        ImGui::Separator();
        ImGui::Text("mouse dx=%d dy=%d  R=%d  imguiMouse=%d",
            in.GetMouseMoveX(), in.GetMouseMoveY(),
            (int)in.GetMouseRButtonCheck(), (int)ImGui::GetIO().WantCaptureMouse);
        if (cam) {
            ImGui::Text("viewMode=%s  atTarget=%d  cine=%d",
                cam->GetViewMode() == ViewMode::Battle ? "BATTLE" : "STRATEGY",
                (int)cam->IsAtTarget(),
                (int)cam->IsCinematic());
            ImGui::Text("azimuth target=%.3f  cur=%.3f", cam->GetTargetAzimuth(), cam->GetAzimuth());
        }
    }

    if (cam) {
        const char* modeName = (cam->GetViewMode() == ViewMode::Battle) ? "BATTLE (TPS)" : "STRATEGY";
        ImGui::Text("Current mode: %s", modeName);
        if (ImGui::Button("Toggle Player <-> Strategy   [F2]", ImVec2(-1, 30))) {
            m_scene.ToggleViewModeDebug();
        }
        if (ImGui::Button("Dive to Battle (player)")) {
            if (player) cam->BeginActorTransition(player->GetSRT().pos, player->GetSRT().rot.y);
        }
        ImGui::SameLine();
        if (ImGui::Button("Enter Strategy")) cam->EnterStrategyView();
    }

    ImGui::Separator();
    bool chg = false;

    if (ImGui::CollapsingHeader("3rd-Person Control / Collision")) {
        ImGui::SliderFloat("Mouse Sens X", &Camera::MOUSE_ORBIT_SENS_X, 0.001f, 0.02f, "%.4f");
        ImGui::SliderFloat("Mouse Sens Y", &Camera::MOUSE_ORBIT_SENS_Y, 0.001f, 0.02f, "%.4f");
        ImGui::SliderFloat("Elev Min", &Camera::ORBIT_ELEV_MIN, -2.55f, -0.5f, "%.3f");
        ImGui::SliderFloat("Elev Max", &Camera::ORBIT_ELEV_MAX, -1.0f, -0.1f, "%.3f");
        ImGui::SliderFloat("Camera Min Height", &Camera::CAMERA_MIN_HEIGHT, 0.0f, 2.0f, "%.2f");
        ImGui::Separator();
        ImGui::SliderFloat("Player Fade Start", &Camera::PLAYER_FADE_START, 0.5f, 6.0f, "%.2f");
        ImGui::SliderFloat("Player Fade Full", &Camera::PLAYER_FADE_FULL, 0.2f, 4.0f, "%.2f");
        if (m_scene.m_player)
            ImGui::Text("Player Fade: %.2f", m_scene.m_player->GetFade());
        if (cam) ImGui::Text("Effective Dist: %.2f", cam->GetEffectiveDistance());
    }

    if (ImGui::CollapsingHeader("Strategy", ImGuiTreeNodeFlags_DefaultOpen)) {
        chg |= ImGui::SliderFloat("Strategy FOV", &Camera::STRATEGY_FOV, 8.0f, 40.0f, "%.1f");
        // 全体表示：BaseView（EnterStrategyView で遷移、ステージ中央を注視）
        chg |= ImGui::SliderFloat("Overview Radius (base)", &Camera::BASE_RADIUS, 10.0f, 60.0f, "%.1f");
        // フォーカス表示：Tracking（BeginActorTransition で行動ユニットを中央に表示）
        chg |= ImGui::SliderFloat("Focus Radius (tracking)", &Camera::ZOOM_RADIUS, 10.0f, 50.0f, "%.1f");
        chg |= ImGui::SliderFloat("Strategy Elevation", &Camera::BASE_ELEVATION, -1.55f, -0.2f, "%.3f");
        if (ImGui::SliderFloat("Strategy Azimuth", &Camera::BASE_AZIMUTH, -3.14159f, 3.14159f, "%.4f")) {
            if (cam) cam->UpdateTargetAzimuth();
        }
    }
    if (ImGui::CollapsingHeader("Enemy Watch Composition")) {
        ImGui::SliderFloat("Back (behind player)", &Camera::ENEMY_WATCH_BACK, 0.0f, 10.0f, "%.2f");
        ImGui::SliderFloat("Shoulder (player L/R)", &Camera::ENEMY_WATCH_SHOULDER, -1.2f, 1.2f, "%.3f");
        ImGui::SliderFloat("Battle Elevation(height)", &Camera::BATTLE_ELEVATION, -1.55f, -0.4f, "%.3f");
    }

    if (ImGui::CollapsingHeader("Transition", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Dwell (center hold)", &Camera::ACTOR_INTRO_HOLD, 0.0f, 2.0f, "%.2f");
        ImGui::TextDisabled("waiting time to TPS MODE");
    }

    // Bound Padding
    if (ImGui::SliderFloat("Bound Padding", &Camera::BOUND_PADDING, -10.0f, 10.0f, "%.1f")) {
        m_scene.RecalculateCameraBounds();
    }

    // 実時間プレビュー：現在モードの構図（半径/仰角/FOV）を再適用
    // ※ 半径反映は Tracking 系状態のとき（BeginActorTransition/Battle 中）に見える
    if (chg && cam) cam->SetViewMode(cam->GetViewMode());

    ImGui::Separator();
    if (ImGui::Button("Save Camera Config", ImVec2(-1, 30))) Camera::SaveConfig();

    ImGui::End();
}

// =========================================================
// F6：Action Cam（攻撃 / キル）
// =========================================================
void GameSceneDebugUI::DrawActionCamPanel() {
    Player* player = m_scene.m_player;
    GameContext* ctx = m_scene.m_context;
    bool isPlayerPhase = ctx && ctx->GetTurnManager() &&
        ctx->GetTurnManager()->GetTurnState() == TurnState::PlayerPhase;

    ImGui::Begin("Action Cam (F6)");

    if (ImGui::CollapsingHeader("Attack Cam", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("AttackZoom Radius", &Camera::ATTACKZOOM_RADIUS, 5.0f, 40.0f, "%.1f");
        ImGui::SliderFloat("AttackZoom Hold", &Camera::ATTACKZOOM_HOLD, 0.1f, 3.0f, "%.2f");
        ImGui::SliderFloat("Attack Lead", &Camera::ATTACK_ZOOM_LEAD, 0.0f, 1.0f, "%.2f");
        if (isPlayerPhase && player && ImGui::Button("Test Attack Cam", ImVec2(-1, 30))) {
            SpawnDebugEnemyInFront(-1);
            player->DebugForceAttack(player->GetFacing());
        }
    }

    if (ImGui::CollapsingHeader("Kill Cam", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("KillCam Radius", &Camera::KILLCAM_RADIUS, -10.0f, 40.0f, "%.1f");
        ImGui::SliderFloat("KillCam Elevation", &Camera::KILLCAM_ELEVATION, -1.5f, 0.0f, "%.3f");
        ImGui::SliderFloat("Shoulder Yaw", &Camera::KILLCAM_SHOULDER_YAW, -1.5f, 1.5f, "%.3f");
        ImGui::SliderFloat("Windup (Lead)", &Camera::KILLCAM_LEAD, 0.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("Wait Timeout", &Camera::KILLCAM_WAIT_TIMEOUT, 0.5f, 4.0f, "%.1f");
        ImGui::SliderFloat("KillCam Hold (slow)", &Camera::KILLCAM_HOLD, 0.2f, 3.0f, "%.2f");
        ImGui::SliderFloat("Time Scale", &Camera::KILLCAM_TIME_SCALE, 0.05f, 1.0f, "%.2f");
        ImGui::SliderFloat("Pitch Lift", &Camera::KILLCAM_PITCH_LIFT, 0.0f, 30.0f, "%.1f");
        ImGui::SliderFloat("Follow Weight", &Camera::KILLCAM_FOLLOW_WEIGHT, 0.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("Follow Max Y", &Camera::KILLCAM_FOLLOW_MAX_Y, 0.0f, 15.0f, "%.1f");
        ImGui::SliderFloat("Follow Min Y", &Camera::KILLCAM_FOLLOW_MIN_Y, -5.0f, 5.0f, "%.1f");
        ImGui::SliderFloat("Return Speed", &Camera::CINE_RETURN_LERP_SPEED, 0.5f, 8.0f, "%.1f");
        ImGui::SliderFloat("Lerp Speed", &Camera::CAMERA_LERP_SPEED, 0.5f, 12.0f, "%.1f");
        ImGui::SliderFloat("Pan Max Dist", &Camera::KILLCAM_PAN_MAX_DIST, 0.0f, 6.0f, "%.1f");
        if (isPlayerPhase && player && ImGui::Button("Test Kill Cam", ImVec2(-1, 30))) {
            SpawnDebugEnemyInFront(1);
            player->DebugForceAttack(player->GetFacing());
        }
    }

    if (!isPlayerPhase) {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Test buttons need Player Phase.");
    }
    ImGui::End();
}

// =========================================================
// F7：Rendering
// =========================================================
void GameSceneDebugUI::DrawRenderingPanel() {
    GameUIManager* ui = m_scene.m_gameUIManager;

    ImGui::Begin("Rendering (F7)");

    if (ImGui::CollapsingHeader("Compare", ImGuiTreeNodeFlags_DefaultOpen)) {
        TOONPARAM tp = Renderer::GetToonParam();
        bool changed = false;

        ImGui::Checkbox("Action UI", &m_scene.m_showActionUI);

        static bool outlineOn = true;
        static float savedW = tp.OutlineColor.w;
        if (ImGui::Checkbox("Outline", &outlineOn)) {
            if (!outlineOn) { savedW = tp.OutlineColor.w; tp.OutlineColor.w = 0.0f; }
            else { tp.OutlineColor.w = savedW; }
            changed = true;
        }

        static bool hemiOn = true;
        static Color savedSky = tp.SkyColor;
        if (ImGui::Checkbox("Hemisphere", &hemiOn)) {
            if (!hemiOn) { savedSky = tp.SkyColor; tp.SkyColor = tp.ShadowColor; }
            else { tp.SkyColor = savedSky; }
            changed = true;
        }

        static bool lightOn = true;
        static Vector4 savedTP = tp.ToonParams;
        if (ImGui::Checkbox("Directional Light", &lightOn)) {
            if (!lightOn) { savedTP = tp.ToonParams; tp.ToonParams.x = 2.0f; tp.ToonParams.y = 2.0f; }
            else { tp.ToonParams = savedTP; }
            changed = true;
        }

        static bool shadowOn = true;
        if (ImGui::Checkbox("Shadow", &shadowOn)) Renderer::s_shadowEnabled = shadowOn;

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
        if (ImGui::SliderFloat("Vignette", &p.Vignette, 0.0f, 2.0f, "%.2f")) Renderer::SetPostFX(p);
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
        float sky[3] = { tp.SkyColor.x, tp.SkyColor.y, tp.SkyColor.z };
        float ground[3] = { tp.ShadowColor.x, tp.ShadowColor.y, tp.ShadowColor.z };
        if (ImGui::ColorEdit3("Sky (up)", sky)) { tp.SkyColor.x = sky[0]; tp.SkyColor.y = sky[1]; tp.SkyColor.z = sky[2]; ch = true; }
        if (ImGui::ColorEdit3("Ground (dn)", ground)) { tp.ShadowColor.x = ground[0]; tp.ShadowColor.y = ground[1]; tp.ShadowColor.z = ground[2]; ch = true; }
        if (ch) Renderer::SetToonParam(tp);
    }

    if (ImGui::CollapsingHeader("HP Bar / Rotate UI")) {
        ImGui::SliderFloat("HP Y Offset", &HPBar::s_hpBarOffsetY, 0.0f, 4.0f, "%.2f");
        ImGui::SliderFloat("HP Heart Size", &HPBar::s_hpBarTexSize, 10.0f, 60.0f, "%.1f");
        ImGui::SliderFloat("HP Heart Gap", &HPBar::s_hpBarGap, 0.0f, 15.0f, "%.1f");
        if (ui) {
            ImGui::Spacing();
            ImGui::Text("Camera Rotate UI (obsolete after Q/E off)");
            ImGui::SliderFloat("Rotate UI X", &ui->GetCameraRotatePos().x, 0.0f, 1920.0f, "%.1f");
            ImGui::SliderFloat("Rotate UI Y", &ui->GetCameraRotatePos().y, 0.0f, 1080.0f, "%.1f");
            ImGui::SliderFloat("Rotate UI Scale", &ui->GetCameraRotateScale(), 0.1f, 3.0f, "%.2f");
        }
    }

    ImGui::End();
}

// =========================================================
// F8：FX
// =========================================================
void GameSceneDebugUI::DrawFxPanel() {
    ImGui::Begin("FX (F8)");

    if (ImGui::CollapsingHeader("FX Params", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderInt("Burst Count", &Fx::Burst.count, 5, 60);
        ImGui::SliderFloat("Burst Speed Max", &Fx::Burst.speedMax, 2.0f, 15.0f);
        ImGui::SliderFloat("Shake Duration", &Fx::DeathShake.duration, 0.1f, 1.0f);
        ImGui::SliderFloat("Star Arm Len", &Fx::Star.armLen, 0.5f, 3.0f);
        ImGui::SliderFloat("Star Progress", &Fx::Star.progress, 0.3f, 0.9f);
        ImGui::SliderFloat("Rubble Bounce", &Fx::Rubble.restitution, 0.0f, 0.9f);

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

        if (ImGui::Button("Save##fx")) Fx::SaveConfig();
        ImGui::SameLine();
        if (ImGui::Button("Load##fx")) Fx::LoadConfig();
    }

    if (ImGui::CollapsingHeader("FX Size")) {
        ImGui::SliderFloat("Rubble Scale Min", &Fx::Rubble.scaleMin, 0.02f, 0.5f);
        ImGui::SliderFloat("Rubble Scale Max", &Fx::Rubble.scaleMax, 0.02f, 0.6f);
        ImGui::SliderFloat3("Hit Spark Scale", &Fx::Hit.sparkScale.x, 0.02f, 0.8f);
        ImGui::SliderFloat("Burst Scale Min", &Fx::Burst.scaleMin, 0.02f, 0.4f);
        ImGui::SliderFloat("Burst Scale Max", &Fx::Burst.scaleMax, 0.02f, 0.5f);
        ImGui::SliderFloat("Trail Scale", &Fx::Trail.scale, 0.02f, 0.5f);
        ImGui::SliderFloat("Star Arm Thick", &Fx::Star.armThick, 0.05f, 0.6f);

        if (m_scene.m_player && m_scene.m_context && m_scene.m_context->GetEffectManager()) {
            EffectManager* fx = m_scene.m_context->GetEffectManager();
            const Vector3 base = m_scene.m_player->GetSRT().pos;
            const float PREVIEW_HIT_Y = 0.6f;
            const float PREVIEW_STAR_Y = 1.2f;
            const float PREVIEW_TRAIL_STEP = 0.35f;

            if (ImGui::Button("Play Rubble")) fx->Spawn3DRubble(base);
            ImGui::SameLine();
            if (ImGui::Button("Play Hit")) { Vector3 p = base; p.y += PREVIEW_HIT_Y; fx->Spawn3DHit(p); }
            ImGui::SameLine();
            if (ImGui::Button("Play Burst")) { Vector3 p = base; p.y += Fx::Burst.spawnYOffset; fx->Spawn3DDeathBurst(p); }
            if (ImGui::Button("Play Star")) { Vector3 p = base; p.y += PREVIEW_STAR_Y; fx->Spawn3DStarCross(p); }
            ImGui::SameLine();
            if (ImGui::Button("Play Trail x10")) {
                for (int i = 0; i < 10; ++i) { Vector3 p = base; p.y += PREVIEW_TRAIL_STEP * i; fx->Spawn3DTrailPuff(p); }
            }
        }
    }

    ImGui::End();
}

// =========================================================
// F9：Debug Actions
// =========================================================
void GameSceneDebugUI::DrawDebugActionsPanel() {
    Player* player = m_scene.m_player;
    Ally* ally = m_scene.m_ally;

    ImGui::Begin("Debug Actions (F9)");

    if (ImGui::Button("Test Win Animation & Text", ImVec2(-1, 30))) {
        if (player) {
            player->StartCelebration();
            if (m_scene.m_camera)
                m_scene.m_camera->ChangeState(CameraState::TargetFocus, player->GetSRT().pos);
        }
    }

    if (ImGui::Button("Test Escape Model & Text", ImVec2(-1, 30))) {
        m_scene.m_shouldShowDebugEscape = !m_scene.m_shouldShowDebugEscape;
        if (m_scene.m_turnCounter) m_scene.m_turnCounter->SetTurn(0);
        if (m_scene.m_shouldShowDebugEscape && ally) {
            m_scene.m_escapeGridX = ally->GetUnitGridX();
            m_scene.m_escapeGridZ = ally->GetUnitGridZ();
        }
    }

    ImGui::End();
}

// =========================================================
// テスト用敵スポーン
// =========================================================
Enemy* GameSceneDebugUI::SpawnDebugEnemyInFront(int hp) {
    Player* m_player = m_scene.m_player;
    GameContext* m_context = m_scene.m_context;
    MapManager* m_mapManager = m_scene.m_mapManager;

    if (!m_player || !m_context || !m_mapManager) return nullptr;

    if (m_debugEnemy) {
        if (m_context->GetEnemyManager())
            m_context->GetEnemyManager()->RemoveEnemy(m_debugEnemy);
        Tile* prev = m_mapManager->GetTile(m_debugEnemy->GetUnitGridX(), m_debugEnemy->GetUnitGridZ());
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