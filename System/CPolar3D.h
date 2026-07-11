#pragma once
#include "CommonTypes.h"

// ３Ｄ極座標系
class CPolar3D {
	float m_radius;				// 半径		
	float m_elevation;			// 仰角
	float m_azimuth;			// 方位角
public:
	CPolar3D() = delete;
	CPolar3D(float radius,
		float elevation,
		float azimuth) : m_radius(radius), m_elevation(elevation), m_azimuth(azimuth) {}
	~CPolar3D() {}

	Vector3 ToCartesian() const{
		Vector3 position;

		position.x = m_radius * sinf(m_elevation) * cosf(m_azimuth);
		position.y = m_radius * cosf(m_elevation);
		position.z = m_radius * sinf(m_elevation) * sinf(m_azimuth);

		return position;
	}
};
