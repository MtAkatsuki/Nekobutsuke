#pragma once
#include <cmath>

// =========================================================
// Direction / DirOffset
// 盤面（グリッド）上の方位と、その (x, z) オフセット表現。
// 座標規約：North=+Z, East=+X, South=-Z, West=-X
// =========================================================
enum class Direction{North,East,South,West};

// 方位を格子座標のオフセット (x, z) として扱うためのヘルパ
struct DirOffset
{
	int x, z;

	// 方位に対応する格子オフセット (x, z) を返す
	static DirOffset From(Direction d)
	{
		switch (d)
		{
		case Direction::North:return { 0,1 };
		case Direction::East:return  { 1,0 };
		case Direction::South:return { 0,-1 };
		case Direction::West:return  { -1,0 };
		default:return{ 0,0 };
		}
	}

	// ベクトル (dx, dz) に最も近い方位を返す（成分の大きい軸を優先）
	static Direction FromVector(float dx, float dz) {
		if (dx == 0.0f && dz == 0.0f) { return Direction::North; }
		else if (std::abs(dx) > std::abs(dz)) { 
			return (dx > 0) ? Direction::East : Direction::West; }
		else{
			return (dz > 0) ? Direction::North : Direction::South;
		}

	}

	// オフセットを回転させた向きを返す（カメラ相対の移動方向変換に使用）
	DirOffset MoveRight()const { return{ z,-x }; }  // 右90度回転
	DirOffset MoveLeft()const { return{ -z,x }; }   // 左90度回転
	DirOffset MoveBack()const { return { -x,-z }; } // 180度反転
};
