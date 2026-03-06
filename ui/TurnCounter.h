#pragma once
#include <vector>
#include <memory>
#include <string>
#include "../system/CSprite.h"
#include "../Application.h"

class TurnCounter {
public:
    TurnCounter() {};
    ~TurnCounter() {};

    void Init() {
		// ターンカウンター用のスプライトを１～５まで読み込む
        for (int i = 1; i <= 5; ++i) {
            std::string path = "assets/texture/ui/turn_" + std::to_string(i) + ".png";
            auto sprite = std::make_unique<CSprite>(462, 94, path.c_str());
            m_numberSprites.push_back(std::move(sprite));
        }
        m_currentTurn = 5;

        m_escapePromptSprite = std::make_unique<CSprite>(628, 269, "assets/texture/ui/escape_prompt.png");
    }

    void SetTurn(int turn) {
		// 範囲は１～５に制限
        if (turn < 0) turn = 0;
        if (turn > 5) turn = 5;
        m_currentTurn = turn;
    }

    void Draw() {
		// インデックス０がターン１、インデックス４がターン５に対応
		// ターン数が０の場合は脱出提示画像を表示する
		//vectorを使っているので、インデックス調整が必要
		Vector3 pos(200.0f, 50.0f, 0.0f);
		Vector3 scale(0.8f, 0.8f, 0.8f);

		if (m_currentTurn <= 0) {
			// ターン数が0の場合、脱出提示画像を表示する
			if (m_escapePromptSprite) {
				MATERIAL mtrl;
				mtrl.Diffuse = Color(1.0f, 1.0f, 1.0f, 1.0f);
				mtrl.TextureEnable = TRUE;
				m_escapePromptSprite->ModifyMtrl(mtrl);
				m_escapePromptSprite->Draw(scale, Vector3(0, 0, 0), pos);
			}
		}
		else {
			// 通常の数字表示
			int index = m_currentTurn - 1;
			if (index >= 0 && index < m_numberSprites.size()) {
				MATERIAL mtrl;
				mtrl.Diffuse = Color(1.0f, 1.0f, 1.0f, 1.0f);
				mtrl.TextureEnable = TRUE;
				m_numberSprites[index]->ModifyMtrl(mtrl);
				m_numberSprites[index]->Draw(scale, Vector3(0, 0, 0), pos);
			}
		}
    }

private:
	// ターンカウンター用のスプライト配列
    std::vector<std::unique_ptr<CSprite>> m_numberSprites;

    std::unique_ptr<CSprite> m_escapePromptSprite;
	// 現在のターン数
    int m_currentTurn = 5;
};