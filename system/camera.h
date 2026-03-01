#pragma once

#include	"commontypes.h"
#include	"renderer.h"
#include	"CPolar3D.h"
#include	"../application.h"

enum class CameraState {
	BaseView,       // 全体俯瞰（特定の対象を追跡しない固定視点）
	Tracking,       // 追跡（プレイヤーやカーソルをフォロー）
	ActionFocus     // アクションフォーカス（攻撃方向の演出用：固定オフセット）
};

class Camera {
protected:
	Vector3	m_position = Vector3(0.0f, 0.0f, 0.0f);	// 実際のカメラ位置 (物理座標)

	// === コーターゲット駆動変数 ===
	Vector3		m_lookat{};				// 現在の注視点
	Vector3		m_targetLookAt{};		// 目標の注視点

	float		m_radius = 30.0f;		// 現在の極座標距離
	float		m_targetRadius = 30.0f;	// 目標距離

	float		m_azimuth = 1.58f;		// 現在の方位角 (左右回転)
	float		m_targetAzimuth = 1.58f;// 目標方位角

	float		m_elevation = -1.08f;	// 現在の仰角 (上下俯仰)
	float		m_targetElevation = -1.08f;

	// 境界制限（バウンディングボックス）
	float		m_minX = -100.0f;
	float		m_maxX = 100.0f;
	float		m_minZ = -100.0f;
	float		m_maxZ = 100.0f;

	// カメラ状態と補間速度
	CameraState	m_state = CameraState::BaseView;
	float		m_lerpSpeed = 5.0f;     // スムーズな移動の速度係数

	Vector3		m_up = { 0,1,0 };
	Matrix4x4	m_viewmtx{};
	Matrix4x4   m_projmtx{};

public:
	virtual ~Camera() {}
	Camera() = default;

	void Init();
	void Dispose();
	void Update(float dt); // === dtパラメータを受け取るように変更 ===
	void Draw();

	// 即時/直接設定 (現在値と目標値を共に上書き)
	void SetPosition(const Vector3& position) { m_position = position; }
	void SetLookat(const Vector3& position) { m_lookat = position; m_targetLookAt = position; }
	void SetUP(const Vector3& up) { m_up = up; }
	void ForceSetPolar(float radius, float azimuth, float elevation) {
		m_radius = radius; m_targetRadius = radius;
		m_azimuth = azimuth; m_targetAzimuth = azimuth;
		m_elevation = elevation; m_targetElevation = elevation;
	}

	// 目標駆動設定（スムーズな移動）
	void SetTargetLookAt(const Vector3& target);
	void SetTargetRadius(float radius) { m_targetRadius = radius; }
	void SetTargetAzimuth(float azimuth) { m_targetAzimuth = azimuth; }
	void SetTargetElevation(float elevation) { m_targetElevation = elevation; }

	// 境界および状態の設定
	void SetBounds(float minX, float maxX, float minZ, float maxZ);
	void SetState(CameraState state) { m_state = state; }
	CameraState GetState() const { return m_state; }

	// ====== 注視点をシーンの中心に即座に合わせる ======
	void SetLookAtCenter() {
		SetLookat(Vector3(SCENE_CENTER_X, SCENE_CENTER_Y, SCENE_CENTER_Z));
	}
	// ====== 目標注視点をシーンの中心へスムーズに移動させる ======
	void SetTargetToCenter() {
		SetTargetLookAt(Vector3(SCENE_CENTER_X, SCENE_CENTER_Y, SCENE_CENTER_Z));
	}

	// ゲッター 
	Matrix4x4 GetViewMatrix() const { return m_viewmtx; }
	Matrix4x4 GetProjMatrix() const { return m_projmtx; }
	Vector3 GetPosition() const { return m_position; }
	Vector3 GetLookat() const { return m_lookat; }
	Vector3 GetUP() const { return m_up; }
	float GetBoundMinX() const { return m_minX; }
	float GetBoundMaxX() const { return m_maxX; }
	float GetBoundMinZ() const { return m_minZ; }
	float GetBoundMaxZ() const { return m_maxZ; }

	static void SaveConfig();
	static void LoadConfig();

public:
	// ==========================================
	// カメラ制御パラメータ定数
	// ==========================================
	static constexpr float TUTORIAL_RADIUS = 45.0f;
	static constexpr float BASE_RADIUS = 30.0f;
	// 【変更】：constexpr を削除し inline を追加。実行時の変更を可能にする
	static inline float ZOOM_RADIUS = 25.0f;
	static inline float BASE_AZIMUTH = 1.58f;
	static inline float BASE_ELEVATION = -1.08f;
	static constexpr float CAMERA_LERP_SPEED = 5.0f;

	// ====== シーン中心座標定数 ======
	static constexpr float SCENE_CENTER_X = 0.0f;
	static constexpr float SCENE_CENTER_Y = 0.0f;
	static constexpr float SCENE_CENTER_Z = 0.0f;

	// ====== 動的境界のエッジバッファ（パディング）定数 ======
	// 【変更】：constexpr を削除し inline を追加。実行時の変更を可能にする
	static inline float BOUND_PADDING = -2.0f;
};