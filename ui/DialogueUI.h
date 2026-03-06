#pragma once
#include "../system/CSprite.h"

class GameContext;

// --- ダイアログの種類を定義する列挙型 ---
enum class DialogueType {
	Help,   // 助けを求める（通常のフェーズ）
	Escape  // 脱出の指示（ターン切れ・脱出開始時）
};

class DialogueUI 
{
public:
	DialogueUI() {};
	~DialogueUI() {};
	void Init(GameContext* context);
	void Update(uint64_t delta);
	void Draw();
	//助けを求める「吹き出し」の表示機能
	void ShowDialogue(const Vector3& targetWorldPos, DialogueType type, float duration = 3.0f);
	//手動で「吹き出し」を消す機能（必要に応じて）
	void HideDialogue();
private:
	GameContext* m_context = nullptr;
	std::unique_ptr<CSprite> m_dialogueHelpSprite;// 助けを求める「吹き出し」のスプライト
	std::unique_ptr<CSprite> m_dialogueEscapeSprite;// 脱出の指示「吹き出し」のスプライト
	CSprite* m_currentSprite = nullptr; // 現在表示中のスプライトを指すポインタ
	bool m_isVisible = false;//「吹き出し」開ける閉じるのフラグ
	float m_timer = 0.0f;//「吹き出し」経った時間
	Vector3 m_targetPos{ 0,0,0 }; // 「吹き出し」は使用者のワールド位置をもとに
};