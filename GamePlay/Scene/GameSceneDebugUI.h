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

    // 毎フレーム入口：ホットキー処理 → 有効なパネルのみ描画
    void Draw();

private:
    void HandlePanelHotkeys();

    // --- 機能別パネル（それぞれ独立したウィンドウ＋独立したトグル）---
    void DrawViewModePanel();     // F5：三人称 / 戦略カメラのパラメータ・停止時間・モード切替
    void DrawActionCamPanel();    // F6：攻撃 / キルカメラ
    void DrawRenderingPanel();    // F7：アウトライン / ライティング / 半球光 / PostFX / Z-Fighting / HP
    void DrawFxPanel();           // F8：エフェクト
    void DrawDebugActionsPanel(); // F9：勝利 / 脱出などのデバッグイベント発火

    // テスト用：プレイヤー前方へデバッグ用Enemyを生成（前回生成分は削除して重複生成を防止）
    Enemy* SpawnDebugEnemyInFront(int hp = -1);

    GameScene& m_scene;             // 非所有（GameScene が本クラスを保持）
    Enemy* m_debugEnemy = nullptr;
    
    // パネル開閉フラグ（F5~F9）
    bool m_showViewMode = true;
    bool m_showActionCam = false;
    bool m_showRendering = false;
    bool m_showFx = false;
    bool m_showDebugActions = false;
};