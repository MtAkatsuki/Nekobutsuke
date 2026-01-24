#pragma once
#include "../system/commontypes.h"

Vector2 WorldToScreen(
	const Vector3& worldPos,
	const Matrix4x4& view,//ƒrƒ…[‹óŠÔ
	const Matrix4x4& proj,//
	int screenWidth,
	int screenHeight
);