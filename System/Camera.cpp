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
	constexpr float FOV_DEG = 15.0f;    // 視野角 (タクティカルRPGに適した狭角設定)
	constexpr float NEAR_PLANE = 0.1f;     // ニアクリップ面
	constexpr float FAR_PLANE = 1000.0f;  // ファークリップ面
	// --- 演出復帰完了の判定閾値（この範囲内で通常カメラへ復帰）---
	constexpr float RETURN_DONE_ANGLE_RAD = 0.02f; // 方位角の許容誤差
	constexpr float RETURN_DONE_DIST_SQ = 0.25f; // 注視点までの許容距離²（0.5m）

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
		{ "KILLCAM_FOLLOW_WEIGHT",  &Camera::KILLCAM_FOLLOW_WEIGHT },
		{ "KILLCAM_FOLLOW_MAX_Y",   &Camera::KILLCAM_FOLLOW_MAX_Y },
		{ "KILLCAM_FOLLOW_MIN_Y",   &Camera::KILLCAM_FOLLOW_MIN_Y },
		{ "CINE_RETURN_LERP_SPEED", &Camera::CINE_RETURN_LERP_SPEED },
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

	// 1. フレームレート非依存の補間係数を計算
	// （演出復帰中は専用の低速補間、それ以外は通常速度）
	const float lerpSpeed = m_cineReturning ? CINE_RETURN_LERP_SPEED : CAMERA_LERP_SPEED;
	float t = 1.0f - std::expf(-lerpSpeed * dt);

	auto LerpFunc = [&](float& current, float target) {
		current += (target - current) * t;
		};

	// 2. 目標パラメータへの追従

	// 線形補間（Lerp）：a + (b - a) * t
	LerpFunc(m_lookat.x, m_targetLookAt.x);
	LerpFunc(m_lookat.y, m_targetLookAt.y);
	LerpFunc(m_lookat.z, m_targetLookAt.z);

	LerpFunc(m_radius, m_targetRadius);
	LerpFunc(m_azimuth, m_targetAzimuth);
	LerpFunc(m_elevation, m_targetElevation);

	// 3. 極座標 → デカルト座標（pitchモード時はカメラ位置を固定し、注視点上昇で見上げのみ）
	if (m_killCamPitch) {
		m_position = m_killCamPos;
		m_up = Vector3(0.0f, 1.0f, 0.0f);   // LookAtLH が水平基準で正しく仰角を作る
	}
	else {
		CPolar3D polor(m_radius, m_elevation, m_azimuth);
		Vector3 offset = polor.ToCartesian();
		m_position = m_lookat + offset;

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
	float fovRad = DirectX::XMConvertToRadians(FOV_DEG);

	m_projmtx = DirectX::XMMatrixPerspectiveFovLH(fovRad, aspectRatio, NEAR_PLANE, FAR_PLANE);
	Renderer::SetProjectionMatrix(&m_projmtx);
}

void Camera::ChangeState(CameraState state, const Vector3& targetPos) {
	if (IsCinematic()) return;// 演出中はカメラ制御を奪わせない（HOLD を最後まで守る）
	m_state = state;

	switch (state) {
	case CameraState::BaseView:
		SetTargetToCenter();
		SetTargetRadius(BASE_RADIUS);
		break;

	case CameraState::Tracking:
		SetTargetLookAt(targetPos);
		SetTargetRadius(ZOOM_RADIUS);
		break;

	case CameraState::TargetFocus:
		SetTargetLookAt(targetPos);
		SetTargetRadius(TARGET_FOCUS_RADIUS);
		break;

	case CameraState::ActionFocus:
		SetTargetLookAt(targetPos);
		SetTargetRadius(ZOOM_RADIUS);
		break;
	}
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

	// 3. 追従率を適用し、注視点を更新
	m_targetLookAt = m_killCamAnchor
		+ Vector3(0.0f, KILLCAM_PITCH_LIFT, 0.0f)
		+ offset * KILLCAM_FOLLOW_WEIGHT;

	// 4. 補間処理は Update() 側で実施
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

