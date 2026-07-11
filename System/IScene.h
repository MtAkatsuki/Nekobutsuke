#pragma once
#include <cstdint>
#include <memory>
#include "../Actor/Base/Gameobject.h"

class GameContext;
// シーンインタフェース
class IScene {
public:
	IScene() = default;
	virtual ~IScene() = default;
	virtual void SetGameContext(GameContext* context) { m_context = context; }
	virtual void update(uint64_t delta) = 0;
	virtual void draw(uint64_t delta) = 0;
	virtual void Init() = 0;
	virtual void dispose() = 0;
protected:
	GameContext* m_context = nullptr;
};
