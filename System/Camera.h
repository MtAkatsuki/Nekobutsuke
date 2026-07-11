#pragma once

#include "CommonTypes.h"
#include "Renderer.h"
#include "CPolar3D.h"
#include "../Core/Application.h"

// --- カメラの追従状態 ---
enum class CameraState {
    BaseView,       // 全体俯瞰（特定の対象を追跡しない固定視点）
    Tracking,       // 追跡（プレイヤーやカーソルをフォロー）
    ActionFocus,    // アクションフォーカス（攻撃方向の演出用など）
    TargetFocus,       // 特写ズーム（チュートリアル/脱出演出時の17.0fなど）
    Cinematic
};

enum class CinePhase { None, AttackZoom, KillLead, KillSlow };

// =========================================================
// Camera クラス
// 3D空間の視点を管理する。
// 極座標系（Polar 3D）を用いたスムーズな追従（Lerp補間）と、
// 4方向のクォータービュー回転制御を提供する。
// =========================================================
class Camera {
public:
    Camera() = default;
    virtual ~Camera() = default;

    // ---------------------------------------------------------
    // ライフサイクル (Lifecycle)
    // ---------------------------------------------------------
    void Init();
    void Dispose();
    void Update(float dt);
    void Draw();

    // ---------------------------------------------------------
    // カメラ制御：直接設定 (Direct Setters)
    // ---------------------------------------------------------
    void SetPosition(const Vector3& position) { m_position = position; }
    void SetUP(const Vector3& up) { m_up = up; }

    // 注視点を即時移動（Lerpを無視して瞬間移動）
    void SetLookat(const Vector3& position) {
        m_lookat = position;
        m_targetLookAt = position;
    }

    // 極座標（距離・方位角・仰角）を即時適用
    void ForceSetPolar(float radius, float azimuth, float elevation) {
        m_radius = radius;    m_targetRadius = radius;
        m_azimuth = azimuth;   m_targetAzimuth = azimuth;
        m_elevation = elevation; m_targetElevation = elevation;
    }

    // ---------------------------------------------------------
    // カメラ制御：目標駆動 (Target-driven Smooth Controls)
    // ※ Lerp補間によって滑らかに遷移するためのターゲット設定
    // ---------------------------------------------------------
    void SetTargetLookAt(const Vector3& target);
    void SetTargetRadius(float radius) { m_targetRadius = radius; }
    void SetTargetAzimuth(float azimuth) { m_targetAzimuth = azimuth; }
    void SetTargetElevation(float elevation) { m_targetElevation = elevation; }

    // シーン中心へのショートカットコマンド
    void SetLookAtCenter() { SetLookat(Vector3(SCENE_CENTER_X, SCENE_CENTER_Y, SCENE_CENTER_Z)); }
    void SetTargetToCenter() { SetTargetLookAt(Vector3(SCENE_CENTER_X, SCENE_CENTER_Y, SCENE_CENTER_Z)); }
	//攻撃演出用のカメラ制御
    void PlayKillCam(const Vector3& attackerPos, const Vector3& victimPos, bool immediate = false);
    void PlayAttackZoom(const Vector3& focusPos);

    // ---------------------------------------------------------
    // カメラ制御：4方向回転 (Quarter-View Rotation)
    // ---------------------------------------------------------
    // 順回転 (Eキー相当)：正面左 -> 正面右 -> 背面右 -> 背面左
    void RotateCameraForward() {
        m_dirIndexOffset++;
        UpdateTargetAzimuth();
    }

    // 逆回転 (Qキー相当)：正面左 -> 背面左 -> 背面右 -> 正面右
    void RotateCameraReverse() {
        m_dirIndexOffset--;
        UpdateTargetAzimuth();
    }

    // 基本カメラ位置(正面左)にリセット
    void ResetCameraDirection() {
        m_dirIndexOffset = 0;
        UpdateTargetAzimuth();
    }

    // 基準方位角(BASE_AZIMUTH)と現在のインデックスに基づく目標角の再計算
    void UpdateTargetAzimuth() {
        m_targetAzimuth = BASE_AZIMUTH + (m_dirIndexOffset * (PI / 2.0f));
    }

    // 負数のオフセットを安全に 0～3 の範囲に正規化（例: -1 -> 3）
    int GetNormalizedDirIndex() const { return (m_dirIndexOffset % 4 + 4) % 4; }

    // ---------------------------------------------------------
    // 状態管理・ゲッター (State & Getters)
    // ---------------------------------------------------------
    void SetBounds(float minX, float maxX, float minZ, float maxZ);
    void ChangeState(CameraState state, const Vector3& targetPos = Vector3(0, 0, 0));
    void UpdateTrackingTarget(const Vector3& targetPos);

    CameraState GetState() const { return m_state; }

    const Matrix4x4& GetViewMatrix() const { return m_viewmtx; }
    const Matrix4x4& GetProjMatrix() const { return m_projmtx; }

    Vector3 GetPosition()     const { return m_position; }
    Vector3 GetLookat()       const { return m_lookat; }
    Vector3 GetUP()           const { return m_up; }
    Vector3 GetTargetLookAt() const { return m_targetLookAt; }

    float GetBoundMinX() const { return m_minX; }
    float GetBoundMaxX() const { return m_maxX; }
    float GetBoundMinZ() const { return m_minZ; }
    float GetBoundMaxZ() const { return m_maxZ; }

    bool IsCinematic() const { return m_cinePhase != CinePhase::None; }
    // 世界の時間スケール（KillSlow 中だけ <1、相机/UI は読まない）
    float GetTimeScale() const { return (m_cinePhase == CinePhase::KillSlow) ? KILLCAM_TIME_SCALE : 1.0f; }
    void CaptureCineReturn();
    void RestoreCineReturn();
    // ---------------------------------------------------------
    // 設定管理 (Config Management)
    // ---------------------------------------------------------
    static void SaveConfig();
    static void LoadConfig();



public:
    // ==========================================
    // カメラ制御パラメータ定数群 (Public Parameters)
    // ==========================================
    static constexpr float TUTORIAL_RADIUS = 45.0f;
    static constexpr float BASE_RADIUS = 30.0f;
    static constexpr float TARGET_FOCUS_RADIUS = 17.0f;
    static constexpr float CAMERA_LERP_SPEED = 5.0f;

    static constexpr float SCENE_CENTER_X = 0.0f;
    static constexpr float SCENE_CENTER_Y = 0.0f;
    static constexpr float SCENE_CENTER_Z = 0.0f;

    // --- 実行時変更可能なパラメータ (inline 変数) ---
    // ※ 設定ファイル(.ini)やデバッグUIから動的に上書きされる値
    static inline float ZOOM_RADIUS = 25.0f;
    static inline float BASE_AZIMUTH = 1.58f;
    static inline float BASE_ELEVATION = -1.08f;
    static inline float BOUND_PADDING = -2.0f;

	// 攻撃演出用のカメラパラメータ
    static inline float KILLCAM_RADIUS = 13.0f;
    static inline float KILLCAM_ELEVATION = -1.35f;  
    static inline float KILLCAM_SHOULDER_YAW = 0.45f; 
    static inline float KILLCAM_HOLD = 1.2f;
    static inline float ATTACKZOOM_RADIUS = 15.0f;
    static inline float ATTACKZOOM_HOLD = 0.7f;
    static inline float KILLCAM_LEAD = 0.25f;
    static inline float KILLCAM_TIME_SCALE = 0.3f;
    static inline float ATTACK_ZOOM_LEAD = 0.15f;
    static inline float KILLCAM_PITCH_LIFT = 9.0f;   // KillSlow：その場で見上げる量（注視点を上へ持ち上げる高さ）

protected:
    // --- トランスフォーム・行列 ---
    Vector3   m_position{ 0.0f, 0.0f, 0.0f };
    Vector3   m_up{ 0.0f, 1.0f, 0.0f };
    Matrix4x4 m_viewmtx{};
    Matrix4x4 m_projmtx{};

    // --- コーターゲット駆動変数 (極座標・注視点) ---
    Vector3 m_lookat{};
    Vector3 m_targetLookAt{};

    float m_radius = 30.0f;
    float m_targetRadius = 30.0f;
    float m_azimuth = 1.58f;
    float m_targetAzimuth = 1.58f;
    float m_elevation = -1.08f;
    float m_targetElevation = -1.08f;

    // --- シネマティック演出用変数 ---
    CinePhase   m_cinePhase = CinePhase::None;
    float       m_cinePhaseTimer = 0.0f;
    bool        m_killCamPitch = false;   // KillSlow：その場見上げモード
    Vector3     m_killCamPos{};           // 固定するカメラ位置
    CameraState m_cineReturnState = CameraState::BaseView;
    Vector3     m_cineReturnLookAt{};
    float       m_cineReturnRadius = BASE_RADIUS;
    float       m_cineReturnAzimuth = BASE_AZIMUTH;
    float       m_cineReturnElevation = BASE_ELEVATION;

    // --- 動作制限・状態 ---
    float m_minX = -100.0f;
    float m_maxX = 100.0f;
    float m_minZ = -100.0f;
    float m_maxZ = 100.0f;

    CameraState m_state = CameraState::BaseView;
    float       m_lerpSpeed = 5.0f;
    int         m_dirIndexOffset = 0; // 回転オフセット：0=基本, 1=右, 2=背後右, 3=背後左
};