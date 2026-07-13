#pragma once
#include <string>
#include <unordered_map>

class IniParser {
public:
	// 設定ファイルを読み込み、浮動小数値のマップとして取得
	static std::unordered_map<std::string, float> LoadAsFloatMap(const std::string& filePath);

	// 浮動小数値のマップを設定ファイルへ保存
	static bool SaveFloatMap(const std::string& filePath, const std::unordered_map<std::string, float>& data);
}; 