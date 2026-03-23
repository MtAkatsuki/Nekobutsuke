#include "GameContext.h"
#include "../GamePlay/Manager/MapManager.h"  
#include "../GamePlay/Manager/TurnManager.h"
#include "../GamePlay/Manager/EnemyManager.h"
#include "../UI/System/DamageNumberManager.h"
#include "../UI/System/GameUIManager.h"
#include "../UI/Component/DialogueUI.h"
#include "../Actor/Character/Player.h" 
#include "../GamePlay/Manager/EffectManager.h"

GameContext::GameContext() = default;
GameContext::~GameContext() = default;

void GameContext::Init() {
    m_mapManager = std::make_unique<MapManager>();
    m_turnManager = std::make_unique<TurnManager>();

    m_enemyManager = std::make_unique<EnemyManager>();
    m_enemyManager->Init(this);

    m_damageNumberManager = std::make_unique<DamageNumberManager>();
    m_damageNumberManager->Init(this);

    m_gameUIManager = std::make_unique<GameUIManager>();
    m_gameUIManager->Init(this);

    m_dialogueUI = std::make_unique<DialogueUI>();
    m_dialogueUI->Init(this);

    m_effectManager = std::make_unique<EffectManager>();
    m_effectManager->Init(this);

    m_camera = std::make_unique<Camera>();
}

MapManager* GameContext::GetMapManager() { return m_mapManager.get(); }
TurnManager* GameContext::GetTurnManager() { return m_turnManager.get(); }
EnemyManager* GameContext::GetEnemyManager() { return m_enemyManager.get(); }
Camera* GameContext::GetCamera() { return m_camera.get(); }

EffectManager* GameContext::GetEffectManager() { return m_effectManager.get(); }
DamageNumberManager* GameContext::GetDamageManager() { return m_damageNumberManager.get(); }
GameUIManager* GameContext::GetUIManager() { return m_gameUIManager.get(); }
DialogueUI* GameContext::GetDialogueUI() { return m_dialogueUI.get(); }

Player* GameContext::GetPlayer() { return m_player; }
void GameContext::SetPlayer(Player* player) { m_player = player; }

Ally* GameContext::GetAlly() { return m_ally; }
void GameContext::SetAlly(Ally* ally) { m_ally = ally; }

TurnState GameContext::GetCurrentTurnState() const {
    return m_turnManager->GetTurnState();
}

