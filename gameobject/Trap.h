#pragma once
#include "MapObject.h"

class Trap : public MapObject
{
public:
    using MapObject::MapObject;

	// Trap（罠）は通行可能
    bool IsWalkable() const override { return true; }

    void Init(MapModelType type, Vector3 position) override;
    void Update(uint64_t delta) override;
	// ユニットが罠に入ったときの処理
    void OnEnter(Unit* unit) override;
    void OnDraw(uint64_t delta) override;
	// 罠の占有サイズを取得（MapManagerでのタイル埋め立て用）
    static void GetDimensions(MapModelType type, int& outW, int& outD);

private:
	bool m_isActivated = false; // 罠が発動したかどうかのフラグ

    std::unique_ptr<CTexture> m_texture;
    int m_sizeX = 1;
    int m_sizeZ = 1;

    // アニメーション関連の変数
    bool m_isAnimating = false;    // 消失アニメーションが再生中かどうか
    float m_animTimer = 0.0f;      // アニメーション経過時間のタイマー
    float m_animDuration = 0.5f;   // アニメーションの総再生時間（秒）
    Vector3 m_initialScale;        // アニメーション開始時の初期スケールを保持
};