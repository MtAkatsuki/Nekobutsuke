#pragma once
#include <cstdint>

// =========================================================
// ゲームコアインターフェース
// Applicationのメインループから呼び出されるグローバル関数群。
// =========================================================
void GameInit();
void GameLoop();
void GameUpdate(uint64_t deltatime);
void GameDraw(uint64_t deltatime);
void GameDispose();