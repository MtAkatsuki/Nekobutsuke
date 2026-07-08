#pragma once

// =========================================================
// GameSceneDebugUI
// デバッグ機能はゲーム進行とは更新頻度・ライフサイクル・利用者が異なるため、
// 専用クラスへ切り出して GameScene をゲーム進行ロジックに専念させる。
// チューニング対象は GameScene の内部状態のため、friend 経由でアクセスする。
// =========================================================
class GameScene;
class Enemy;

class GameSceneDebugUI {
public:
    explicit GameSceneDebugUI(GameScene& scene) : m_scene(scene) {}

    // Debug UI の描画
    void DrawCameraTuningWindow();  
    void DrawGridDebugText();       

private:
    // テスト用：プレイヤー前方へデバッグ用Enemyを生成（前回生成分は削除して重複生成を防止）
    Enemy* SpawnDebugEnemyInFront(int hp = -1);

    GameScene& m_scene;             // 非所有（GameScene が本クラスを保持）
    Enemy* m_debugEnemy = nullptr;
    bool m_enabled = true;
};