#include "CSVParser.h"
#include <fstream>
#include <sstream>
#include <cassert>
#include "../../Core/DebugLog.h"
// CSVデータのパース処理
std::vector<std::vector<std::string>> CSVParser::ReadAsGrid(const std::string& filePath) {
	// 解析後のデータを格納する2次元ベクトル
	std::vector<std::vector<std::string>> grid;

	// ファイルストリームを開く
	std::ifstream file(filePath);

	// 【エラー検出】失敗を即座に通知する Fail Fast 設計
	if (!file.is_open()) {
		// 1. エラーログを出力し、実行環境に関係なく原因調査用の情報を残す
		DBG_ERROR("[CSVParser] CRITICAL ERROR: Failed to open CSV file: " << filePath);

		// 2. 開発中は assert で処理を停止し、パス設定ミスを早期発見する
		assert(false && "Failed to open CSV file! Check the path or working directory.");

		// 3. リリース時は空データを返却し、上位層で安全にエラー処理できるようにする
		return grid;
	}

	std::string line;
	// ファイルの終端に達するまで、1行ずつ読み込みを繰り返す
	// (file から 1行取り出し、変数 line に格納する)
	while (std::getline(file, line)) {

		// 現在処理している行のセルデータを格納するための、一時的な配列（1次元ベクトル）
		std::vector<std::string> row;

		// 読み込んだ 1行の文字列(line)を、ストリームとして扱うための変換
		// これにより、文字列をファイルと同じように「流し込み」や「区切り読み」ができるようになる
		std::stringstream ss(line);

		// 分割された個々のセル（カンマ間のデータ）を一時的に保持する変数
		std::string cell;

		// カンマ（,）を区切り文字として各セルを抽出
		while (std::getline(ss, cell, ',')) {
			// --- セル内の不要な空白文字（スペース、タブ、改行等）を除去（トリミング） ---

			// 先頭の空白を削除
			// (スペース ' ', タブ '\t', 回車 '\r', 改行 '\n' 以外の文字を探す)
			cell.erase(0, cell.find_first_not_of(" \t\r\n"));
			// 末尾の空白を削除
			cell.erase(cell.find_last_not_of(" \t\r\n") + 1);

			// クリーニング済みのデータを現在の行に追加
			row.push_back(cell);
		}
		// 完成した行をグリッドデータに追加
		grid.push_back(row);
	}

	// 解析結果を返す
	return grid;
}