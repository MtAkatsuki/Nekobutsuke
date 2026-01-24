#pragma once
enum class Direction{North,East,South,West};

struct DirOffset
{
	int x, z;

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

	static Direction FromVector(float dx, float dz) {
		if (dx == 0.0f && dz == 0.0f) { return Direction::North; }
		else if (std::abs(dx) > std::abs(dz)) { 
			return (dx > 0) ? Direction::East : Direction::West; }
		else{
			return (dz > 0) ? Direction::North : Direction::South;
		}

	}

	DirOffset MoveRight()const { return{ z,-x }; }
	DirOffset MoveLeft()const { return{ -z,x }; }
	DirOffset MoveBack()const { return { -x,-z }; }
};
