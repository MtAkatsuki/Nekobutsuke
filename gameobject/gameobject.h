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
		m_WorldMatrix = Matrix4x4::CreateScale(m_srt.scale)
			* Matrix4x4::CreateRotationY(m_srt.rot.y)
			* Matrix4x4::CreateTranslation(m_srt.pos);
	}

	void Draw(uint64_t delta) { 
		Renderer::SetWorldMatrix(&m_WorldMatrix);
		OnDraw(delta);

	};

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
	virtual void OnDraw(uint64_t delta) {};
	GameContext* m_context;
	bool m_isDead = false;
};