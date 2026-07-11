#pragma once
#include "commontypes.h"

// ３Ｄ極座標系
class CPolor3D {
	float m_radius;				// 半径		
	float m_elevation;			// 仰角
	float m_azimuth;			// 方位角
public:
	CPolor3D() = delete;
	CPolor3D(float radius,
		float elevation,
		float azimuth) : m_radius(radius), m_elevation(elevation), m_azimuth(azimuth) {}
	~CPolor3D() {}

	Vector3 ToCartesian() const{
		Vector3 position;

		position.x = m_radius * sinf(m_elevation) * cosf(m_azimuth);
		position.y = m_radius * cosf(m_elevation);
		position.z = m_radius * sinf(m_elevation) * sinf(m_azimuth);

		return position;
	}
};
