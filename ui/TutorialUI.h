#pragma once
#include "../system/CSprite.h"
#include <vector>
#include <memory>

/**
 * @brief チュートリアルUI管理クラス
 * ゲーム開始前にチュートリアル画像を順番に表示する役割を担当します
 */
class TutorialUI {
public:
    TutorialUI();
    ~TutorialUI();

    // 初期化およびリソースのロード
    void Init();

    // 更新ロジック（true が返った場合は全チュートリアル再生完了）
    // deltatime: 秒（注意：計算を容易にするため、ここでは秒単位で渡すか）
    void Update(float deltaSeconds);

    // 描画
    void Draw();

    // 全て再生終了したかどうか
    bool IsAllFinished() const { return m_isAllFinished; }

private:
    // チュートリアルの状態
    enum class State {
        APPEARING,    // 拡大して出現
        WAITING,      // 入力待ち
        DISAPPEARING, // 縮小して消失
        FINISHED      // 現在の画像終了
    };

    std::vector<std::unique_ptr<CSprite>> m_images; // 画像リソースリスト
    int m_currentIndex = 0;                         // 現在表示中の画像インデックス

    State m_state = State::APPEARING;               // 現在の状態
    bool m_isAllFinished = false;                   // 全ての画像の再生が完了したか

    float m_scale = 0.0f;       // 現在のスケール比率 (0.0 ~ 1.0)
    float m_animSpeed = 5.0f;   // アニメーションの拡縮速度

    // 画面中心座標（プロジェクトによっては 1920/2, 1080/2 など、実際の状況に応じて調整が必要）
    // Application.h や main.h で画面サイズが定義されていると仮定し、ここでは一時的にハードコードするか Init で取得してください
    float m_centerX = 640.0f;
    float m_centerY = 360.0f;
};