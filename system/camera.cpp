#include "commonTypes.h"
#include "renderer.h"
#include "camera.h"
#include "../application.h"
#include <algorithm>
#include <fstream>
#include <iostream>

void Camera::SetTargetLookAt(const Vector3& target) {
	m_targetLookAt = target;
	// 注視点をクランプし、カメラが追跡しすぎて画面外の未描画領域（穿面）が見えるのを防ぐ
	m_targetLookAt.x = std::clamp(m_targetLookAt.x, m_minX, m_maxX);
	m_targetLookAt.z = std::clamp(m_targetLookAt.z, m_minZ, m_maxZ);
}

//視野の境界線設置する
void Camera::SetBounds(float minX, float maxX, float minZ, float maxZ) {
	m_minX = minX;
	m_maxX = maxX;
	m_minZ = minZ;
	m_maxZ = maxZ;
}

void Camera::Init()
{
}

void Camera::Dispose()
{

}

	void Camera::Update(float dt) {
		// 1. フレームレートに依存しない平滑化係数 t を計算
		// m_lerpSpeed が大きいほど追従が速くなる（推奨値：5.0f - 20.0f）
		// t = 1 - f^dt
		// f = e^ln(f)-> f^dt = e^ln(f)*dt
		float t = 1.0f - std::expf(-m_lerpSpeed * dt);

		// 2. 平滑化係数を使用して注視点を更新
		// 線形補間（Lerp）：a + (b - a) * t
		auto LerpFunc = [&](float& current, float target) {
			current += (target - current) * t;
			};

		LerpFunc(m_lookat.x, m_targetLookAt.x);
		LerpFunc(m_lookat.y, m_targetLookAt.y);
		LerpFunc(m_lookat.z, m_targetLookAt.z);

		// 3. 極座標パラメータを更新
		LerpFunc(m_radius, m_targetRadius);
		LerpFunc(m_azimuth, m_targetAzimuth);
		LerpFunc(m_elevation, m_targetElevation);

		// 4. 位置と UP ベクトルを再計算（ロジックは維持）
		CPolor3D polor(m_radius, m_elevation, m_azimuth);
		Vector3 offset = polor.ToCartesian();
		m_position = m_lookat + offset;

		CPolor3D polorup(1.0f, m_elevation + PI / 2.0f, m_azimuth);
		m_up = polorup.ToCartesian();
	}

void Camera::Draw()
{
	// ビュー変換後列作成
	m_viewmtx = 
		DirectX::XMMatrixLookAtLH(
			m_position, 
			m_lookat, 
			m_up);				// 左手系にした　20230511 by suzuki.tomoki

	// DIRECTXTKのメソッドは右手系　20230511 by suzuki.tomoki
	// 右手系にすると３角形頂点が反時計回りになるので描画されなくなるので注意
	// このコードは確認テストのために残す
	//	m_ViewMatrix = m_ViewMatrix.CreateLookAt(m_Position, m_Target, up);					

	Renderer::SetViewMatrix(&m_viewmtx);

	//プロジェクション行列の生成
	constexpr float fieldOfView = DirectX::XMConvertToRadians(15.0f);    // 視野角
	
	float aspectRatio = static_cast<float>(Application::GetWidth()) / static_cast<float>(Application::GetHeight());	// アスペクト比	
	float nearPlane = 0.1f;       // ニアクリップ
	float farPlane = 1000.0f;      // ファークリップ

	//プロジェクション行列の生成
	m_projmtx =
		DirectX::XMMatrixPerspectiveFovLH(
			fieldOfView, 
			aspectRatio, 
			nearPlane, 
			farPlane);	// 左手系にした　20230511 by suzuki.tomoki

	// DIRECTXTKのメソッドは右手系　20230511 by suzuki.tomoki
	// 右手系にすると３角形頂点が反時計回りになるので描画されなくなるので注意
	// このコードは確認テストのために残す
//	projectionMatrix = DirectX::SimpleMath::Matrix::CreatePerspectiveFieldOfView(fieldOfView, aspectRatio, nearPlane, farPlane);

	Renderer::SetProjectionMatrix(&m_projmtx);
}

// 補助関数：文字列の先頭と末尾から空白、タブ、改行などを取り除く
std::string Trim(const std::string& s) {
	// 1. 最初に出現する【空白以外】の文字の位置を検索
	auto start = s.find_first_not_of(" \t\r\n");

	// 2. 最後に出現する【空白以外】の文字の位置を検索
	auto end = s.find_last_not_of(" \t\r\n");

	// 3. 判定および切り出し
	// 空白以外の文字が見つからない場合（文字列全体が空白か空の場合）は、空の文字列 "" を返す
	return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
}

// ====== 設定をローカルに保存 ======
void Camera::SaveConfig() {
	// プロジェクトの .exe と同階層に nekobutsuke_camera.ini を生成
	std::ofstream file("nekobutsuke_camera.ini");
	if (file.is_open()) {
		file << "ZOOM_RADIUS=" << ZOOM_RADIUS << "\n";
		file << "BOUND_PADDING=" << BOUND_PADDING << "\n";
		file << "BASE_AZIMUTH=" << BASE_AZIMUTH << "\n"; 
		file << "BASE_ELEVATION=" << BASE_ELEVATION << "\n"; 
		file.close();
		std::cerr << "[Camera] Config Saved to nekobutsuke_camera.ini" << std::endl;
	}
}

// ====== ローカルから設定を読み込み ======
void Camera::LoadConfig() {
	std::ifstream file("nekobutsuke_camera.ini");
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
				if (key == "ZOOM_RADIUS") ZOOM_RADIUS = value;
				else if (key == "BOUND_PADDING") BOUND_PADDING = value;
				else if (key == "BASE_AZIMUTH") BASE_AZIMUTH = value;
				else if (key == "BASE_ELEVATION") BASE_ELEVATION = value; 
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

