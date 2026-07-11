#pragma once
#include <cstdint>
#include <memory>
#include "../Actor/Base/GameObject.h"

class GameContext;
// シーンインタフェース
class IScene {
public:
	IScene() = default;
	virtual ~IScene() = default;
	virtual void SetGameContext(GameContext* context) { m_context = context; }
	virtual void Update(uint64_t delta) = 0;
	virtual void Draw(uint64_t delta) = 0;
	virtual void Init() = 0;
	virtual void Dispose() = 0;
protected:
	GameContext* m_context = nullptr;
};
