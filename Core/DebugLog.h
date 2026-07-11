#pragma once

// =========================================================
// デバッグログ facility
//  DBG_TRACE : Debug ビルドのみ出力（進捗・トレース）。Release では消滅
//  DBG_ERROR : 全ビルドで出力（致命的エラー・警告）
// ※ .cpp からのみ include すること（ヘッダに入れると iostream が伝播する）
// =========================================================
#include <iostream>

#ifdef _DEBUG
#define DBG_TRACE(msg) do { std::cerr << msg << std::endl; } while (0)
#else
#define DBG_TRACE(msg) ((void)0)
#endif

#define DBG_ERROR(msg) do { std::cerr << msg << std::endl; } while (0)