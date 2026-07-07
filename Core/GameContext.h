#pragma once

#include <memory>
#include "../EnumClass/TurnState.h"

class MapManager;
class TurnManager;
class EnemyManager;
class Player;
class Ally;
class DamageNumberManager;
class GameUIManager;
class DialogueUI;
class Camera;
class EffectManager;

// =========================================================
// GameContext クラス
// ゲームプレイ中の主要なシステム・マネージャー群への参照を保持する
// 依存性の注入（Dependency Injection）コンテナとして機能し、
// オブジェクト間の直接的な結合（スパゲッティコード）を防止する
// =========================================================
class GameContext {
public:
	GameContext();
	~GameContext();

	void Init();

	// ---------------------------------------------------------
	// コアシステムマネージャーへのアクセス (Core Managers)
	// ---------------------------------------------------------
	MapManager* GetMapManager();
	TurnManager* GetTurnManager();
	EnemyManager* GetEnemyManager();
	Camera* GetCamera();

	// ---------------------------------------------------------
	// エフェクト・UIマネージャーへのアクセス (Presentation Managers)
	// ---------------------------------------------------------
	EffectManager* GetEffectManager();
	DamageNumberManager* GetDamageManager();
	GameUIManager* GetUIManager();
	DialogueUI* GetDialogueUI();

	// ---------------------------------------------------------
	// コアエンティティへのアクセス (Core Entities)
	// ---------------------------------------------------------
	Player* GetPlayer();
	void SetPlayer(Player* player);

	Ally* GetAlly();
	void SetAlly(Ally* ally);

	// ---------------------------------------------------------
	// ステートショートカット (State Shortcuts)
	// ---------------------------------------------------------
	TurnState GetCurrentTurnState() const;

private:
	// =========================================================
	// メンバー変数 (Managers & Entity Pointers)
	// =========================================================
	std::unique_ptr<MapManager> m_mapManager;
	std::unique_ptr<TurnManager> m_turnManager;
	std::unique_ptr<EnemyManager> m_enemyManager;
	std::unique_ptr<DamageNumberManager> m_damageNumberManager;
	std::unique_ptr<Camera> m_camera;
	std::unique_ptr<GameUIManager> m_gameUIManager;
	std::unique_ptr<DialogueUI> m_dialogueUI;
	std::unique_ptr<EffectManager> m_effectManager;

	// ライフサイクルをScene等に委譲している実体への弱参照（Raw Pointers）
	Player* m_player = nullptr;
	Ally* m_ally = nullptr;
};