#pragma once
#include <cstdint>
#include	"../system/transform.h"
#include	"../system/renderer.h"
#include <stdexcept>

class GameContext;
class GameObject {
public:
	GameObject() = delete;
	//明確なcontextがあるコンストラクタを提供する
	explicit GameObject(GameContext* context) :
		m_context(context) 
	{
		if(!m_context)
		{
			throw std::invalid_argument("GameContext pointer is null");
		}
	}

	virtual ~GameObject() = default;
	virtual void Update(uint64_t delta) = 0;

	void UpdateWorldMatrix() 
	{
		Matrix4x4 rotationMatrix = Matrix4x4::CreateRotationZ(m_srt.rot.z)
			* Matrix4x4::CreateRotationX(m_srt.rot.x)
			* Matrix4x4::CreateRotationY(m_srt.rot.y);

		m_WorldMatrix = Matrix4x4::CreateScale(m_srt.scale)
			* rotationMatrix
			* Matrix4x4::CreateTranslation(m_srt.pos);
	}

	// 描画順：3. 床面ヒント UI レイヤー (地面に接地)
	void DrawFloorUI(uint64_t delta) {
		Renderer::SetWorldMatrix(&m_WorldMatrix);
		OnDrawFloorUI(delta);
	}

	// 描画順：5.1 不透明エンティティレイヤー
	void Draw(uint64_t delta) {
		Renderer::SetWorldMatrix(&m_WorldMatrix);
		OnDraw(delta);
	};


	// 描画順：5.3 半透明エンティティレイヤー (残像など)
	void DrawTransparent(uint64_t delta) {
		Renderer::SetWorldMatrix(&m_WorldMatrix);
		OnDrawTransparent(delta);
	}

	// 描画順：6. 攻撃プレビューヒントレイヤー (浮遊・デプス無視)
	void DrawOverlay(uint64_t delta) {
		Renderer::SetWorldMatrix(&m_WorldMatrix);
		OnDrawOverlay(delta);
	}

	// 浮遊UI、矢印、パスなどのOverlay（オーバーレイ）要素専用の描画メソッド
	void DrawOverlayLayer(uint64_t delta) {
		Renderer::SetWorldMatrix(&m_WorldMatrix);
		OnDrawOverlay(delta);
	}

	virtual void init() = 0;
	virtual void dispose() = 0;

	SRT getSRT() const{
		return m_srt;
	}

	void	setSRT(const SRT& srt) {
		m_srt = srt;
	}

	void	setPosition(const Vector3& pos) {
		m_srt.pos = pos;
	}

	bool IsDead() const { return m_isDead; }
	void Destroy() { m_isDead = true; }

protected:
	SRT		m_srt{};
	Matrix4x4 m_WorldMatrix;

	virtual void OnDrawFloorUI(uint64_t delta) {};
	virtual void OnDraw(uint64_t delta) {};
	virtual void OnDrawTransparent(uint64_t delta) {};
	virtual void OnDrawOverlay(uint64_t delta) {};

	GameContext* m_context;
	bool m_isDead = false;
};