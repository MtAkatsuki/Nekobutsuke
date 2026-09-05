#include "CommonTypes.h"
#include "Renderer.h"
#include "Camera.h"
#include "Utility/IniParser.h"
#include "../Core/Application.h"
#include "../Core/DebugLog.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>

namespace {
	// --- レンダリング・投影行列用定数 ---
	constexpr float NEAR_PLANE = 0.1f;     // ニアクリップ面
	constexpr float FAR_PLANE = 1000.0f;  // ファークリップ面
	// --- 演出復帰完了の判定閾値（この範囲内で通常カメラへ復帰）---
	constexpr float RETURN_DONE_ANGLE_RAD = 0.05f; // 方位角の許容誤差（尾の緩慢さを抑えるため緩和）
	constexpr float RETURN_DONE_DIST_SQ = 1.0f;    // 注視点までの許容距離²（0.5→1.0m：末端の低速走行を短縮）

	// --- 設定ファイル名 ---
	const std::string CONFIG_FILE_NAME = "nekobutsuke_camera.ini";

	// --- 設定エントリ表：キー名と変数の対応）---
	struct ConfigEntry {
		const char* key;
		float* value;
	};

	const ConfigEntry CONFIG_TABLE[] = {
		{ "ZOOM_RADIUS",          &Camera::ZOOM_RADIUS },
		{ "BOUND_PADDING",        &Camera::BOUND_PADDING },
		{ "BASE_AZIMUTH",         &Camera::BASE_AZIMUTH },
		{ "BASE_ELEVATION",       &Camera::BASE_ELEVATION },
		{ "KILLCAM_RADIUS",       &Camera::KILLCAM_RADIUS },
		{ "KILLCAM_ELEVATION",    &Camera::KILLCAM_ELEVATION },
		{ "KILLCAM_SHOULDER_YAW", &Camera::KILLCAM_SHOULDER_YAW },
		{ "KILLCAM_LEAD",         &Camera::KILLCAM_LEAD },
		{ "KILLCAM_HOLD",         &Camera::KILLCAM_HOLD },
		{ "KILLCAM_TIME_SCALE",   &Camera::KILLCAM_TIME_SCALE },
		{ "ATTACKZOOM_RADIUS",    &Camera::ATTACKZOOM_RADIUS },
		{ "ATTACKZOOM_HOLD",      &Camera::ATTACKZOOM_HOLD },
		{ "ATTACK_ZOOM_LEAD",     &Camera::ATTACK_ZOOM_LEAD },
		{ "KILLCAM_PITCH_LIFT",   &Camera::KILLCAM_PITCH_LIFT },
		{ "CAMERA_LERP_SPEED",      &Camera::CAMERA_LERP_SPEED },
		{ "LOOKAT_LERP_SPEED",    &Camera::LOOKAT_LERP_SPEED },
		{ "KILLCAM_FOLLOW_WEIGHT",  &Camera::KILLCAM_FOLLOW_WEIGHT },
		{ "KILLCAM_FOLLOW_MAX_Y",   &Camera::KILLCAM_FOLLOW_MAX_Y },
		{ "KILLCAM_FOLLOW_MIN_Y",   &Camera::KILLCAM_FOLLOW_MIN_Y },
		{ "CINE_RETURN_LERP_SPEED", &Camera::CINE_RETURN_LERP_SPEED },
		{ "KILLCAM_PAN_MAX_DIST",   &Camera::KILLCAM_PAN_MAX_DIST },
		{ "STRATEGY_FOV",         &Camera::STRATEGY_FOV },
		{ "BATTLE_FOV",           &Camera::BATTLE_FOV },
		{ "BATTLE_RADIUS",        &Camera::BATTLE_RADIUS },
		{ "BATTLE_ELEVATION",     &Camera::BATTLE_ELEVATION },
		{ "ACTOR_INTRO_HOLD",     &Camera::ACTOR_INTRO_HOLD },
		{ "BASE_RADIUS",          &Camera::BASE_RADIUS },
		{ "BATTLE_LOOK_Y_OFFSET", &Camera::BATTLE_LOOK_Y_OFFSET },
		{ "MOUSE_ORBIT_SENS_X",   &Camera::MOUSE_ORBIT_SENS_X },
		{ "MOUSE_ORBIT_SENS_Y",   &Camera::MOUSE_ORBIT_SENS_Y },
		{ "ORBIT_ELEV_MIN",       &Camera::ORBIT_ELEV_MIN },
		{ "ORBIT_ELEV_MAX",       &Camera::ORBIT_ELEV_MAX },
		{ "CAMERA_MIN_HEIGHT",    &Camera::CAMERA_MIN_HEIGHT },
		{ "PLAYER_FADE_START", &Camera::PLAYER_FADE_START },
		{ "PLAYER_FADE_FULL",  &Camera::PLAYER_FADE_FULL },
		{ "ENEMY_WATCH_BACK",     &Camera::ENEMY_WATCH_BACK },
		{ "ENEMY_WATCH_SHOULDER", &Camera::ENEMY_WATCH_SHOULDER },
		{ "AIM_ORBIT_MAX_AZ",        &Camera::AIM_ORBIT_MAX_AZ },
		{ "AIM_ORBIT_MAX_EL",        &Camera::AIM_ORBIT_MAX_EL },
		{ "AIM_ORBIT_RETURN_DELAY",  &Camera::AIM_ORBIT_RETURN_DELAY },
	};

	float UnwrapNear(float ref, float angle) {
		float d = angle - ref;
		while (d > PI) { angle -= 2.0f * PI; d -= 2.0f * PI; }
		while (d < -PI) { angle += 2.0f * PI; d += 2.0f * PI; }
		return angle;
	}
}

void Camera::Init()
{
}

void Camera::Dispose()
{

}

void Camera::Update(float dt) {

	if (m_cinePhase != CinePhase::None) {
		m_cinePhaseTimer += dt;
		switch (m_cinePhase) {
		case CinePhase::AttackZoom:
			if (m_cinePhaseTimer >= ATTACKZOOM_HOLD) RestoreCineReturn();
			break;
		case CinePhase::KillLead:
			// 肩越し構図へ移行しながら、対象の死亡飛翔開始を待機する。
			// 死亡飛翔が開始された時点で KillSlow へ遷移する。
			// 一定時間内に開始されない場合は演出を終了する。
			if (m_cinePhaseTimer >= KILLCAM_WAIT_TIMEOUT) RestoreCineReturn();
			break;
		case CinePhase::KillSlow:
			if (m_cinePhaseTimer >= KILLCAM_HOLD) RestoreCineReturn();
			break;
		default: break;
		}
	}
	// 演出復帰中は、目標位置まで十分に収束したら通常追従へ戻す
	if (m_cineReturning) {
		const bool azimuthArrived = std::fabsf(m_targetAzimuth - m_azimuth) < RETURN_DONE_ANGLE_RAD;
		const bool lookatArrived = (m_targetLookAt - m_lookat).LengthSquared() < RETURN_DONE_DIST_SQ;
		if (azimuthArrived && lookatArrived) m_cineReturning = false;
	}

	// 行動者トランジション：戦略で居中 → 一定時間後に第三人称へ俯冲
	if (m_actorIntroActive && !IsCinematic()) {
		m_actorIntroTimer += dt;
		if (m_actorIntroTimer >= ACTOR_INTRO_HOLD) {
			m_actorIntroActive = false;
			EnterBattleView(m_actorIntroPos, m_actorIntroYaw); // TPS視点へ急降下（背後への回り込みを含む）
		}
	}

	// 1. フレームレート非依存の補間係数を計算
	// （演出復帰中は専用の低速補間、それ以外は通常速度）
	const float lerpSpeed = m_cineReturning ? CINE_RETURN_LERP_SPEED : CAMERA_LERP_SPEED;
	float t = 1.0f - std::expf(-lerpSpeed * dt);

	// 注視点の高速追従は「第三人称の実時間追従(Battle+Tracking)」時のみ。
	// 戦略へ戻る/居中などの転場は通常速度(CAMERA_LERP_SPEED)でゆっくり。
	bool tpsFollow = (m_viewMode == ViewMode::Battle
		&& m_state == CameraState::Tracking
		&& !m_cineReturning);
	float tLook = tpsFollow ? (1.0f - std::expf(-LOOKAT_LERP_SPEED * dt)) : t;

	auto LerpFunc = [&](float& current, float target) {
		current += (target - current) * t;
		};

	// 2. 目標パラメータへの追従

	// 注視点は tLook（密着）
	m_lookat.x += (m_targetLookAt.x - m_lookat.x) * tLook;
	m_lookat.y += (m_targetLookAt.y - m_lookat.y) * tLook;
	m_lookat.z += (m_targetLookAt.z - m_lookat.z) * tLook;

	LerpFunc(m_radius, m_targetRadius);
	LerpFunc(m_azimuth, m_targetAzimuth);
	LerpFunc(m_elevation, m_targetElevation);
	LerpFunc(m_fov, m_targetFov);

	// 3. 極座標 → デカルト座標（pitchモード時はカメラ位置を固定し、注視点上昇で見上げのみ）
	if (m_killCamPitch) {
		m_position = m_killCamPos;
		m_up = Vector3(0.0f, 1.0f, 0.0f);   // LookAtLH が水平基準で正しく仰角を作る
	}
	else {
		// 極座標 → カメラ位置
		CPolar3D polor(m_radius, m_elevation, m_azimuth);
		Vector3 offset = polor.ToCartesian();

		// 床による衝突判定（地面への貫通を防止）：
		// カメラが床より低くなる場合、「注視点 → カメラ」のレイ上を移動させ、床面まで引き上げる。
		// lookAt はプレイヤーの胸元（床より高い位置）にあるため、
		// 交点はプレイヤー側に寄り、カメラはそれ以上下降せず、地面に沿って接近する。
		Vector3 camPos = m_lookat + offset;
		if (camPos.y < CAMERA_MIN_HEIGHT && offset.y < -0.0001f) {
			float t = (CAMERA_MIN_HEIGHT - m_lookat.y) / offset.y; // offset.y < 0 かつ分子 < 0 → t ∈ (0,1)
			t = std::clamp(t, 0.05f, 1.0f);
			offset *= t;                                           // レイを短縮し、床面の高さまで引き上げる＋プレイヤーに接近
			camPos = m_lookat + offset;
		}

		m_effectiveDist = offset.Length();
		m_position = camPos;

		CPolar3D polorup(1.0f, m_elevation + PI / 2.0f, m_azimuth);
		m_up = polorup.ToCartesian();
	}
	
}

void Camera::Draw(){
	// 左手系ビュー行列の生成
	m_viewmtx = DirectX::XMMatrixLookAtLH(m_position, m_lookat, m_up);
	Renderer::SetViewMatrix(&m_viewmtx);

	// プロジェクション行列の生成
	float aspectRatio = static_cast<float>(Application::GetWidth()) / static_cast<float>(Application::GetHeight());
	float fovRad = DirectX::XMConvertToRadians(m_fov);

	m_projmtx = DirectX::XMMatrixPerspectiveFovLH(fovRad, aspectRatio, NEAR_PLANE, FAR_PLANE);
	Renderer::SetProjectionMatrix(&m_projmtx);
}


void Camera::EnterStrategyView() {
	m_viewMode = ViewMode::Strategy;
	ChangeState(CameraState::BaseView);
}

void Camera::EnterBattleView(const Vector3& focusPos, float facingYaw) {
	m_viewMode = ViewMode::Battle;
	ChangeState(CameraState::Tracking, focusPos);
	OrientBehind(facingYaw);
}

void Camera::HomeToStrategy() {
	if (IsCinematic()) return;
	m_viewMode = ViewMode::Strategy;
	m_state = CameraState::Tracking;      // 現在の注視対象を維持（ステージ中央には戻さない）
	m_targetLookAt = m_lookat;            // 注視点を現在の対象に固定：ズームアウト中も以前の対象を注視（#1）
	m_targetRadius = BASE_RADIUS;         // 戦略俯瞰まで引き戻す
	m_targetElevation = BASE_ELEVATION;
	m_targetFov = STRATEGY_FOV;
}

void Camera::BeginActorTransition(const Vector3& focusPos, float facingYaw) {
	if (IsCinematic()) return;

	// 第1段階：戦略画面の俯瞰視点で、これから行動するユニットを中央に捉える
	m_viewMode = ViewMode::Strategy;
	ChangeState(CameraState::Tracking, focusPos);

	// 第2段階：一定時間停止した後、第三人称視点へカメラを急降下させる
	m_actorIntroActive = true;
	m_actorIntroTimer = 0.0f;
	m_actorIntroPos = focusPos;
	m_actorIntroYaw = facingYaw;
}

void Camera::OrientBehind(float facingYaw) {
	if (IsCinematic()) return;
	m_targetAzimuth = UnwrapNear(m_azimuth, atan2f(cosf(facingYaw), sinf(facingYaw)));
}

void Camera::ApplyFraming() {
	switch (m_state) {
	case CameraState::BaseView:    SetTargetToCenter(); m_targetRadius = BASE_RADIUS;        break;
	case CameraState::TargetFocus: m_targetRadius = TARGET_FOCUS_RADIUS;                     break;
	case CameraState::Tracking:
	case CameraState::ActionFocus: m_targetRadius = GetTrackingRadius();                     break;
	default: return;  
	}
	m_targetElevation = GetTrackingElevation();  
	m_targetFov = GetTrackingFov();
}

void Camera::ChangeState(CameraState state, const Vector3& targetPos) {
	if (IsCinematic()) return;
	m_state = state;
	if (state != CameraState::BaseView) SetTargetLookAt(targetPos);
	ApplyFraming();
}

void Camera::SetViewMode(ViewMode mode) {
	if (IsCinematic()) return;
	m_viewMode = mode;
	ApplyFraming();
}

void Camera::UpdateTrackingTarget(const Vector3& targetPos) {
	if (IsCinematic()) return;   // 演出中は追従で注視点を上書きしない
	if (m_state == CameraState::Tracking ||
		m_state == CameraState::TargetFocus ||
		m_state == CameraState::ActionFocus) {
		SetTargetLookAt(targetPos);
	}
}

void Camera::SetTargetLookAt(const Vector3& target) {
	m_targetLookAt = target;
	// 第三人称視点：注視点を胸～頭の高さまで引き上げる（戦略 / 俯瞰視点では適用しない）
	if (m_viewMode == ViewMode::Battle) {
		m_targetLookAt.y += BATTLE_LOOK_Y_OFFSET;
	}
	// 注視点をクランプし、カメラが追跡しすぎて画面外の未描画領域（穿面）が見えるのを防ぐ
	m_targetLookAt.x = std::clamp(m_targetLookAt.x, m_minX, m_maxX);
	m_targetLookAt.z = std::clamp(m_targetLookAt.z, m_minZ, m_maxZ);
}

void Camera::SetBounds(float minX, float maxX, float minZ, float maxZ) {
	m_minX = minX;
	m_maxX = maxX;
	m_minZ = minZ;
	m_maxZ = maxZ;
}

void Camera::SaveConfig() {
	// プロジェクトの .exe と同階層に nekobutsuke_camera.ini を生成
	std::unordered_map<std::string, float> config;

	// 1. テーブル駆動でキーと値のマップを自動構築する
	for (const auto& entry : CONFIG_TABLE) {
		config[entry.key] = *(entry.value);
	}

	// 2. 下層（IniParser）へ書き込みを委譲
	if (IniParser::SaveFloatMap(CONFIG_FILE_NAME, config)) {
		DBG_TRACE("[Camera] Configuration saved to disk.");
	}
	else {
		DBG_ERROR("[Camera] Failed to save configuration to disk.");
	}
}

void Camera::LoadConfig() {
	auto config = IniParser::LoadAsFloatMap(CONFIG_FILE_NAME);

	if (config.empty()) {
		DBG_ERROR("[Camera] Config is empty or missing. Using hardcoded default parameters.");
		return;
	}

	// テーブル駆動方式による設定値の自動反映
	for (const auto& entry : CONFIG_TABLE) {
		// INI 内に定義されたキーが存在する場合、対応する変数へ設定値を適用
		if (config.find(entry.key) != config.end()) {
			*(entry.value) = config[entry.key];
		}
	}

	DBG_TRACE("[Camera] Configuration loaded successfully via Table-Driven Method.");
}

void Camera::CaptureCineReturn() {
	if (m_cinePhase != CinePhase::None) return;
	m_cineReturnState = m_state;
	m_cineReturnLookAt = m_targetLookAt;
	m_cineReturnRadius = m_targetRadius;
	m_cineReturnAzimuth = m_targetAzimuth;
	m_cineReturnElevation = m_targetElevation;
}

void Camera::RestoreCineReturn() {
	// pitchモードから戻る時、現在のカメラ位置を起点に通常モデルへ連続接続（ポップ防止）
	if (m_killCamPitch) {
		CPolar3D polor(m_radius, m_elevation, m_azimuth);
		m_lookat = m_killCamPos - polor.ToCartesian();  // pos = lookat+offset が m_killCamPos と一致
		m_killCamPitch = false;
	}

	m_cinePhase = CinePhase::None;
	m_cinePhaseTimer = 0.0f;
	m_state = m_cineReturnState;
	m_targetLookAt = m_cineReturnLookAt;
	m_targetRadius = m_cineReturnRadius;
	m_targetAzimuth = m_cineReturnAzimuth;
	m_targetElevation = m_cineReturnElevation;
	// 演出復帰を開始（復帰速度・完了判定は Update 側で制御）
	m_cineReturning = true;
}

void Camera::PlayKillCam(const Vector3& attackerPos, const Vector3& victimPos, bool immediate) {
	if (m_cinePhase == CinePhase::KillLead || m_cinePhase == CinePhase::KillSlow) return;
	CaptureCineReturn();

	m_cineReturning = false;     // 前回の演出復帰状態をクリア
	m_killCamAnchor = victimPos; // 新しい追従アンカーを設定

	Vector3 d = victimPos - attackerPos; d.y = 0.0f;
	if (d.LengthSquared() < 0.0001f) d = Vector3(0, 0, 1); else d.Normalize();

	float targetAzim = atan2f(d.z, d.x) + KILLCAM_SHOULDER_YAW;
	targetAzim = UnwrapNear(m_azimuth, targetAzim);

	m_targetLookAt = victimPos;
	m_targetRadius = KILLCAM_RADIUS;
	m_targetAzimuth = targetAzim;
	m_targetElevation = KILLCAM_ELEVATION;

	m_state = CameraState::Cinematic;

	if (immediate) {
		// 反応式（敵/環境/味方の致死）：肩越しポーズへ瞬間スナップし、即・死亡聚焦(KillSlow)へ
		m_lookat = m_targetLookAt;
		m_radius = m_targetRadius;
		m_azimuth = m_targetAzimuth;
		m_elevation = m_targetElevation;

		// この肩越しポーズのカメラ位置を確定（pitchモードの基準位置）
		CPolar3D polor(m_radius, m_elevation, m_azimuth);
		m_position = m_lookat + polor.ToCartesian();

		// KillLead を飛ばして直接 KillSlow（その場見上げ）
		BeginKillSlow();
	}
	else {
		// 予測式（自軍攻撃）：KILLCAM_LEAD かけて肩越しへ寄せる（蓄勢）
		m_cinePhase = CinePhase::KillLead;
		m_cinePhaseTimer = 0.0f;
	}
}

void Camera::BeginKillSlow() {
	// カメラ位置を固定したまま、注視点を上方向へ補正する
	m_cinePhase = CinePhase::KillSlow;
	m_cinePhaseTimer = 0.0f;
	m_killCamPitch = true;
	m_killCamPos = m_position;
	m_targetLookAt = m_killCamAnchor + Vector3(0.0f, KILLCAM_PITCH_LIFT, 0.0f);
}

void Camera::UpdateKillCamFollow(const Vector3& victimPos) {
	// 死亡飛翔開始時に KillSlow（見上げ＋ソフト追従）へ遷移
	if (m_cinePhase == CinePhase::KillLead) BeginKillSlow();
	if (m_cinePhase != CinePhase::KillSlow) return;

	// 1. 演出開始位置（アンカー）からの移動量を取得
	Vector3 offset = victimPos - m_killCamAnchor;

	// 2. Y方向の追従量を制限し、過度な見上げや地面方向への追従を防ぐ
	offset.y = std::clamp(offset.y, KILLCAM_FOLLOW_MIN_Y, KILLCAM_FOLLOW_MAX_Y);

	// 3. 水平方向（X/Z）の追従量を緩やかに制限する。
	//    狭い FOV（15°）では大きなパン移動が前進しているように見えるため、
	//    小さい移動量は比例的に追従し、移動量が大きくなるほど増加量を抑える。
	//    KILLCAM_PAN_MAX_DIST を上限とし、0 の場合は従来どおり水平追従を行わない。
	const float horizDist = sqrtf(offset.x * offset.x + offset.z * offset.z);
	if (horizDist > 0.001f && KILLCAM_PAN_MAX_DIST > 0.01f) {
		const float mapped = KILLCAM_PAN_MAX_DIST *
			(1.0f - std::expf(-KILLCAM_FOLLOW_WEIGHT * horizDist / KILLCAM_PAN_MAX_DIST));
		const float scale = mapped / horizDist;   // 方向を保ったまま長さのみを縮小
		offset.x *= scale;
		offset.z *= scale;
	}
	else {
		offset.x = 0.0f;
		offset.z = 0.0f;
	}

	// 4. 注視点を更新（Y方向は追従率を適用し、X/Z方向は制限後の値を使用）
	m_targetLookAt = m_killCamAnchor
		+ Vector3(0.0f, KILLCAM_PITCH_LIFT, 0.0f)
		+ Vector3(offset.x, offset.y * KILLCAM_FOLLOW_WEIGHT, offset.z);

	// 5. 補間処理は Update() 側で実施
}

void Camera::PlayAttackZoom(const Vector3& focusPos) {
	CaptureCineReturn();
	m_cineReturning = false;
	m_targetLookAt = focusPos; 
	m_targetRadius = ATTACKZOOM_RADIUS;
	m_state = CameraState::Cinematic;
	m_cinePhase = CinePhase::AttackZoom;
	m_cinePhaseTimer = 0.0f;
}

void Camera::OrbitByMouse(float dAzimuth, float dElevation) {
	m_targetAzimuth += dAzimuth;
	m_azimuth = m_targetAzimuth;

	m_targetElevation = std::clamp(m_targetElevation + dElevation, ORBIT_ELEV_MIN, ORBIT_ELEV_MAX);
	m_elevation = m_targetElevation;
}

void Camera::FrameEnemyFromPlayer(const Vector3& playerPos, const Vector3& enemyPos) {
	if (IsCinematic()) return;
	m_viewMode = ViewMode::Battle;
	m_state = CameraState::Tracking;

	// 敵を中央に配置：注視点 = 敵（BATTLE_LOOK_Y_OFFSET による戦闘時の高さ補正を含む）
	SetTargetLookAt(enemyPos);

	// 水平方向：プレイヤー → 敵
	float dx = enemyPos.x - playerPos.x;
	float dz = enemyPos.z - playerPos.z;
	float dist = sqrtf(dx * dx + dz * dz);

	// 方位角：プレイヤー → 敵の方向を基準に、肩越しのオフセットを加えてプレイヤーを画面の一側に配置
	float az = atan2f(dz, dx) + ENEMY_WATCH_SHOULDER;
	m_targetAzimuth = UnwrapNear(m_azimuth, az);

	m_targetElevation = BATTLE_ELEVATION;   // カメラの高さ・仰角を制御
	m_targetFov = BATTLE_FOV;

	// 半径：カメラを水平方向にプレイヤーの背後 BACK の位置へ配置
	// カメラと敵の水平距離 = プレイヤー→敵の距離 + BACK
	// また、水平距離 = radius * |sin(elev)|
	float sinE = fabsf(sinf(m_targetElevation));
	if (sinE < 0.2f) sinE = 0.2f;            // 仰角が水平に近い場合の半径の発散を防止
	m_targetRadius = (dist + ENEMY_WATCH_BACK) / sinE;
}

void Camera::BeginAimFollow() {
	m_aimOrbitOffsetAz = 0.0f;
	m_aimOrbitOffsetEl = 0.0f;
	m_aimIdleTimer = 0.0f;
}

void Camera::AimFollow(const Vector3& playerPos, const Vector3& enemyPos,
	float mouseDx, float mouseDy, float dt) {
	if (IsCinematic()) return;
	m_viewMode = ViewMode::Battle;
	m_state = CameraState::Tracking;

	// --- アンカー：FrameEnemyFromPlayer と同じ「敵中央・玩家背後」の構図 ---
	SetTargetLookAt(enemyPos);
	float dx = enemyPos.x - playerPos.x;
	float dz = enemyPos.z - playerPos.z;
	float dist = sqrtf(dx * dx + dz * dz);
	float anchorAz = atan2f(dz, dx) + ENEMY_WATCH_SHOULDER;
	float anchorEl = BATTLE_ELEVATION;

	// --- マウスによる受限オフセット ---
	bool hasInput = (mouseDx != 0.0f || mouseDy != 0.0f);
	if (hasInput) {
		// 左へ動かす→右回り（FREE_MOVE と反転を揃える）。感度は既存 SENS を流用
		m_aimOrbitOffsetAz = std::clamp(m_aimOrbitOffsetAz - mouseDx * MOUSE_ORBIT_SENS_X,
			-AIM_ORBIT_MAX_AZ, AIM_ORBIT_MAX_AZ);
		m_aimOrbitOffsetEl = std::clamp(m_aimOrbitOffsetEl + mouseDy * MOUSE_ORBIT_SENS_Y,
			-AIM_ORBIT_MAX_EL, AIM_ORBIT_MAX_EL);
		m_aimIdleTimer = 0.0f;
	}
	else {
		// マウス静止：一定時間後、オフセットを 0 へ滑らかに戻す（帰位）
		m_aimIdleTimer += dt;
		if (m_aimIdleTimer >= AIM_ORBIT_RETURN_DELAY) {
			float t = 1.0f - std::expf(-CAMERA_LERP_SPEED * dt);
			m_aimOrbitOffsetAz += (0.0f - m_aimOrbitOffsetAz) * t;
			m_aimOrbitOffsetEl += (0.0f - m_aimOrbitOffsetEl) * t;
		}
	}

	// --- 最終ターゲット = アンカー + オフセット ---
	m_targetAzimuth = UnwrapNear(m_targetAzimuth, anchorAz + m_aimOrbitOffsetAz);
	m_targetElevation = std::clamp(anchorEl + m_aimOrbitOffsetEl, ORBIT_ELEV_MIN, ORBIT_ELEV_MAX);
	m_targetFov = BATTLE_FOV;

	// 半径：FrameEnemyFromPlayer と同じ「距離＋背後オフセット」換算
	float sinE = fabsf(sinf(m_targetElevation));
	if (sinE < 0.2f) sinE = 0.2f;
	m_targetRadius = (dist + ENEMY_WATCH_BACK) / sinE;
}

bool Camera::IsAtTarget() const {
	if (fabsf(m_radius - m_targetRadius) > 0.3f)  return false;
	if (fabsf(m_elevation - m_targetElevation) > 0.02f) return false;
	if (fabsf(m_fov - m_targetFov) > 0.5f) return false;

	// 方位角はラップアラウンドを考慮して差分を求める
	float da = m_targetAzimuth - m_azimuth;
	while (da > PI) da -= 2.0f * PI;
	while (da < -PI) da += 2.0f * PI;
	if (fabsf(da) > 0.03f) return false;

	// 注視点（許容誤差：0.5m）
	if ((m_targetLookAt - m_lookat).LengthSquared() > 0.25f) return false;

	return true;
}