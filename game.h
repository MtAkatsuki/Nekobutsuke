#pragma once
#include <cstdint>

// =========================================================
// ゲームコアインターフェース
// Applicationのメインループから呼び出されるグローバル関数群。
// =========================================================
void gameinit();
void gameloop();
void gameupdate(uint64_t deltatime);
void gamedraw(uint64_t deltatime);
void gamedispose();