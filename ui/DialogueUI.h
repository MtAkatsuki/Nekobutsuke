#pragma once
#include "../system/CSprite.h"

class GameContext;

class DialogueUI 
{
public:
	DialogueUI() {};
	~DialogueUI() {};
	void Init(GameContext* context);
	void Update(uint64_t delta);
	void Draw();
	//助けを求める「吹き出し」の表示機能
	void ShowDialogue(const Vector3& targetWorldPos, float duration = 2.0f);

private:
	GameContext* m_context = nullptr;
	std::unique_ptr<CSprite> m_dialogueSprite;//スパライト　メンバー

	bool m_isVisible = false;//「吹き出し」開ける閉じるのフラグ
	float m_timer = 0.0f;//「吹き出し」経った時間
	Vector3 m_targetPos{ 0,0,0 }; // 「吹き出し」は使用者のワールド位置をもとに
};