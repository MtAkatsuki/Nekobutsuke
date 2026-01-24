#pragma once
#include <vector>
#include <algorithm>
#include <memory>
#include "../gameobject/enemy.h"

class GameContext;

enum class EnemyPhaseState {
    IDLE,          
    READY_TO_START, 
    ACTING,        
    INTERVAL,       
    ALL_FINISHED    
};

class EnemyManager {
public:
    EnemyManager() = default;
    ~EnemyManager() = default;

    void Init(GameContext* context) { m_context = context; }
    void RegisterEnemy(Enemy* enemy);
    void RemoveEnemy(Enemy* enemy);
    void StartEnemyPhase();

    void Update(uint64_t dt);
    bool IsFinished() const { return m_state == EnemyPhaseState::ALL_FINISHED; }
    bool EnemyIsAllDead() const;
	bool EnemyIsEmpty() const { return m_enemies.empty(); }
    bool IsAnyEnemyDying() const;
    bool IsAnyEnemyAnimating() const;
    void ClearAll();
private:
    void ResortAndRenumber();

    GameContext* m_context = nullptr;
    std::vector<Enemy*> m_enemies;

    EnemyPhaseState m_state = EnemyPhaseState::IDLE;
    int m_currentActorIndex = 0;
    float m_timer = 0.0f;
    const float ACT_INTERVAL = 0.5f;
};