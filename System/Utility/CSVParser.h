#pragma once
#include <vector>
#include <string>

// =========================================================
// CSVParser クラス
// CSV ファイルを行×列の文字列グリッドとして読み込むユーティリティ。
// =========================================================
class CSVParser {
public:
    // CSV を 2 次元配列として読み込む（各セルは前後の空白を除去。開けない場合は空を返す）
    static std::vector<std::vector<std::string>> ReadAsGrid(const std::string& filePath);
};