#include "CommonTypes.h"
#include "Renderer.h"
#include "Camera.h"
#include "../Core/Application.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>

namespace {
	// --- レンダリング・投影行列用定数 ---
	constexpr float FOV_DEG = 15.0f;    // 視野角 (タクティカルRPGに適した狭角設定)
	constexpr float NEAR_PLANE = 0.1f;     // ニアクリップ面
	constexpr float FAR_PLANE = 1000.0f;  // ファークリップ面

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
			if (m_cinePhaseTimer >= KILLCAM_LEAD) {
				m_cinePhase = CinePhase::KillSlow;
				m_cinePhaseTimer = 0.0f;
				// その場で見上げる：カメラ位置を固定し、注視点だけ上へ振る（Pitchのみ）
				m_killCamPitch = true;
				m_killCamPos = m_position;
				m_targetLookAt = m_lookat + Vector3(0.0f, KILLCAM_PITCH_LIFT, 0.0f);  // 注視点を上へ
			}
			break;
		case CinePhase::KillSlow:
			if (m_cinePhaseTimer >= KILLCAM_HOLD) RestoreCineReturn();
			break;
		default: break;
		}
	}

	// 1. フレームレートに依存しない平滑化（Lerp）係数の計算

	// t = 1 - f^dt

	// f = e^ln(f)-> f^dt = e^ln(f)*dt
	float t = 1.0f - std::expf(-m_lerpSpeed * dt);

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
		CPolor3D polor(m_radius, m_elevation, m_azimuth);
		Vector3 offset = polor.ToCartesian();
		m_position = m_lookat + offset;

		CPolor3D polorup(1.0f, m_elevation + PI / 2.0f, m_azimuth);
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

std::string Trim(const std::string& s) {
	// 補助関数：文字列の先頭と末尾から空白、タブ、改行などを取り除く
	
	// 1. 最初に出現する【空白以外】の文字の位置を検索
	auto start = s.find_first_not_of(" \t\r\n");

	// 2. 最後に出現する【空白以外】の文字の位置を検索
	auto end = s.find_last_not_of(" \t\r\n");

	// 3. 判定および切り出し
	// 空白以外の文字が見つからない場合（文字列全体が空白か空の場合）は、空の文字列 "" を返す
	return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
}

void Camera::SaveConfig() {
	// プロジェクトの .exe と同階層に nekobutsuke_camera.ini を生成
	std::ofstream file(CONFIG_FILE_NAME);
	if (file.is_open()) {
		for (const auto& e : CONFIG_TABLE) {
			file << e.key << "=" << *e.value << "\n";
		}
		file.close();
		std::cerr << "[Camera] Config Saved to " << CONFIG_FILE_NAME << std::endl;
	}
}

void Camera::LoadConfig() {
	std::ifstream file(CONFIG_FILE_NAME);
	if (!file.is_open()) {
		std::cerr << "[Camera] Warning: Could not open config file." << std::endl;
		return;
	}

	std::string line;
	while (std::getline(file, line)) {
		// 1. 先頭と末尾の空白を除去
		line = Trim(line);

		// 2. 空行、または # や ; で始まるコメント行をスキップ
		if (line.empty() || line[0] == '#' || line[0] == ';') {
			continue;
		}

		// 3. 等号（=）を検索
		size_t delimiterPos = line.find('=');
		if (delimiterPos != std::string::npos) {
			// 分割し、さらに key と value をクレンジング（空白除去）
			//substr(start, length) :
			// key: インデックス 0 から開始し、等号（=）の直前までを切り出す。
			// valueStr: 等号の次の位置から開始し、行末までをすべて切り出す。
			std::string key = Trim(line.substr(0, delimiterPos));
			std::string valueStr = Trim(line.substr(delimiterPos + 1));

			try {
				float value = std::stof(valueStr);
				// 表を線形探索してキー一致の変数へ書き込む（14件なので十分速い）
				for (const auto& e : CONFIG_TABLE) {
					if (key == e.key) {
						*e.value = value;
						break;
					}
				}
			}
			catch (const std::exception& e) {
				// 数値への変換に失敗した場合のみエラーを出力
				std::cerr << "[Camera] Invalid value for " << key << ": " << valueStr << std::endl;
			}
		}
	}
	file.close();
	std::cerr << "[Camera] Config Loaded Successfully." << std::endl;
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
		CPolor3D polor(m_radius, m_elevation, m_azimuth);
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
}

void Camera::PlayKillCam(const Vector3& attackerPos, const Vector3& victimPos, bool immediate) {
	if (m_cinePhase == CinePhase::KillLead || m_cinePhase == CinePhase::KillSlow) return;
	CaptureCineReturn();

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
		CPolor3D polor(m_radius, m_elevation, m_azimuth);
		m_position = m_lookat + polor.ToCartesian();

		// KillLead を飛ばして直接 KillSlow（その場見上げ）
		m_cinePhase = CinePhase::KillSlow;
		m_cinePhaseTimer = 0.0f;
		m_killCamPitch = true;
		m_killCamPos = m_position;
		m_targetLookAt = m_lookat + Vector3(0.0f, KILLCAM_PITCH_LIFT, 0.0f);
	}
	else {
		// 予測式（自軍攻撃）：KILLCAM_LEAD かけて肩越しへ寄せる（蓄勢）
		m_cinePhase = CinePhase::KillLead;
		m_cinePhaseTimer = 0.0f;
	}
}

void Camera::PlayAttackZoom(const Vector3& focusPos) {
	CaptureCineReturn();
	m_targetLookAt = focusPos; 
	m_targetRadius = ATTACKZOOM_RADIUS;
	m_state = CameraState::Cinematic;
	m_cinePhase = CinePhase::AttackZoom;
	m_cinePhaseTimer = 0.0f;
}

