#pragma once

#include "../Base/MapObject.h"

class CShader;

// =========================================================
// Trap クラス
// マップ上の罠ギミック。ユニットが進入(OnEnter)した際にダメージを与え、
// 発動後はアニメーションを伴って消滅する。
// =========================================================
struct Tile;
class Trap : public MapObject {
public:
	using MapObject::MapObject;

	// ---------------------------------------------------------
	// ライフサイクル (Lifecycle)
	// ---------------------------------------------------------
	void Init(MapModelType type, Vector3 position) override;
	void Update(float deltaSeconds) override;

	// ---------------------------------------------------------
	// レンダリング (Rendering)
	// ---------------------------------------------------------
	void OnDraw(float deltaSeconds) override;

	// ---------------------------------------------------------
	// イベント・状態 (Events & Status)
	// ---------------------------------------------------------
	void OnEnter(Unit* unit) override;

	// タイル上に「未発動の罠」があればそれを返す（なければ nullptr）
	static Trap* GetArmedTrap(const Tile* tile);
	bool IsActivated() const { return m_hasActivated; }
	int GetTrapDamage() const { return m_trapDamage; }

private:
	bool m_hasActivated = false;   // 罠が発動済みかどうかのフラグ
	int m_trapDamage = 5;          // トラップの基本ダメージ量

	int m_sizeX = 1;
	int m_sizeZ = 1;

	// --- アニメーション制御 ---
	bool m_isDisappearing = false; // 消失アニメーション再生中フラグ
	float m_animTimer = 0.0f;      // アニメーション進行タイマー
	Vector3 m_initialScale;        // 開始時の基準スケール

	CShader* m_toonShader = nullptr;
};