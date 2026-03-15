#pragma once
#include <cstdint>
#include <stdexcept>
#include "../system/transform.h"
#include "../system/renderer.h"

class GameContext;

// =========================================================
// GameObject クラス
// ゲーム内すべての一時的・永続的エンティティの基底クラス
// 空間トランスフォームと描画レイヤー（Zオーダー）の基礎を提供する
// =========================================================
class GameObject {
public:
	GameObject() = delete;

	// 依存性の注入：明確なGameContextを要求し、nullの場合は例外を投げる（防御的プログラミング）
	explicit GameObject(GameContext* context) : m_context(context) {
		if (!m_context) {
			throw std::invalid_argument("GameContext pointer is null");
		}
	}

	virtual ~GameObject() = default;

	virtual void init() = 0;
	virtual void Update(uint64_t delta) = 0;
	virtual void dispose() = 0;

	// ---------------------------------------------------------
	// レンダリングパイプライン呼び出し口
	// ---------------------------------------------------------

	void UpdateWorldMatrix() {
		Matrix4x4 rotationMatrix = Matrix4x4::CreateRotationZ(m_srt.rot.z)
			* Matrix4x4::CreateRotationX(m_srt.rot.x)
			* Matrix4x4::CreateRotationY(m_srt.rot.y);

		m_WorldMatrix = Matrix4x4::CreateScale(m_srt.scale)
			* rotationMatrix
			* Matrix4x4::CreateTranslation(m_srt.pos);
	}

	// 描画順 3: 床面ヒント UI レイヤー (地面に接地、トラップの下)
	void DrawFloorUI(uint64_t delta) {
		Renderer::SetWorldMatrix(&m_WorldMatrix);
		OnDrawFloorUI(delta);
	}

	// 描画順 5.1: 不透明エンティティレイヤー (キャラクター本体など)
	void Draw(uint64_t delta) {
		Renderer::SetWorldMatrix(&m_WorldMatrix);
		OnDraw(delta);
	}

	// 描画順 5.3: 半透明エンティティレイヤー (残像、エフェクトなど)
	void DrawTransparent(uint64_t delta) {
		Renderer::SetWorldMatrix(&m_WorldMatrix);
		OnDrawTransparent(delta);
	}

	// 描画順 6: 攻撃プレビューヒントレイヤー (最前面3DUI、デプス無視)
	void DrawOverlay(uint64_t delta) {
		Renderer::SetWorldMatrix(&m_WorldMatrix);
		OnDrawOverlay(delta);
	}

	// ---------------------------------------------------------
	// ゲッター / セッター
	// ---------------------------------------------------------
	SRT getSRT() const { return m_srt; }
	void setSRT(const SRT& srt) { m_srt = srt; }
	void setPosition(const Vector3& pos) { m_srt.pos = pos; }

	bool IsDead() const { return m_isDead; }
	void Destroy() { m_isDead = true; }

protected:
	// --- サブクラスで実装する具体的な描画ロジック ---
	virtual void OnDrawFloorUI(uint64_t delta) {}
	virtual void OnDraw(uint64_t delta) {}
	virtual void OnDrawTransparent(uint64_t delta) {}
	virtual void OnDrawOverlay(uint64_t delta) {}

	GameContext* m_context;
	SRT m_srt{};
	Matrix4x4 m_WorldMatrix;
	bool m_isDead = false;
};