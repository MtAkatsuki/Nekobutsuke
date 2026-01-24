#pragma once
#include <vector>
#include <string>
#include <map>
#include <memory>
#include "../system/CSprite.h"

class GameContext;
class Camera;

struct UVQuad{
Vector2 tex[4];
};

struct ActiveDamageNumber {
	Vector3 startWorldPos;
	Vector3 currentOffset;
	int number;
	float timer;

	float timePhase1;
	float timePhase2; 
	float totalTime; 
};

class DamageNumberManager {
	public:
		DamageNumberManager() {};
		~DamageNumberManager() {};

		void Init(GameContext* context);
		void Update(uint64_t dt);
		void Draw();
		void SpawnDamage(Vector3 position, int damage);
		void ClearAll() { m_activeNumbers.clear(); }
private:
	std::unique_ptr<CSprite> m_sprite; 
	std::map<char, UVQuad> m_numberUVs;
	std::vector<ActiveDamageNumber> m_activeNumbers;

	const float FLOAT_SPEED = 0.5f; 
	const float CHAR_WIDTH = 14.0f;
	GameContext* m_context = nullptr;
	
};