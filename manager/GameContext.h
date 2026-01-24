#pragma once
#include <memory>
#include "../enum class/TurnState.h"
#include "../manager/EnemyManager.h"
#include <iostream>

class MapManager;
class TurnManager;
class Player;
class Ally;
class DamageNumberManager;
class GameUIManager;
class DialogueUI;
class Camera;


class GameContext
{
public:
	GameContext();
	~GameContext();
	MapManager* GetMapManager();
	TurnManager* GetTurnManager();
	Camera* GetCamera();
	
	// ’‡ŠÔ‚ÌSet‚ÆGet‚Ì’Ç‰Á
	void SetAlly(Ally* ally);
	Ally* GetAlly();
	// ƒvƒŒƒCƒ„[‚ÌGet‚ÆSet
	Player* GetPlayer();
	void SetPlayer(Player* player);

	EnemyManager* GetEnemyManager();
	DamageNumberManager* GetDamageManager();
	GameUIManager* GetUIManager();
	DialogueUI* GetDialogueUI();

	TurnState GetCurrentTurnState() const;

	void Init();

private:

	std::unique_ptr<MapManager> m_mapManager;
	std::unique_ptr<TurnManager> m_turnManager;
	std::unique_ptr<EnemyManager> m_enemyManager;
	std::unique_ptr<DamageNumberManager> m_damageNumberManager;
	std::unique_ptr<Camera> m_camera;
	std::unique_ptr<GameUIManager> m_gameUIManager;
	std::unique_ptr<DialogueUI> m_dialogueUI;
	Player* m_player = nullptr;
	Ally* m_ally = nullptr;
};