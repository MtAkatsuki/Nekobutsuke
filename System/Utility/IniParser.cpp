#include "IniParser.h"
#include <fstream>
#include <sstream>
#include "../../Core/DebugLog.h"

std::unordered_map<std::string, float> IniParser::LoadAsFloatMap(const std::string& filePath) {
    std::unordered_map<std::string, float> configMap;
    std::ifstream file(filePath);

    if (!file.is_open()) {
        DBG_ERROR("[IniParser] CRITICAL: Failed to open config file: " << filePath);
        // Fail Fast により異常を通知し、上位層で安全に復旧可能な状態を返却
        return configMap;
    }

    std::string line;
    while (std::getline(file, line)) {
        // 空行・コメント行（# / ; 始まり）をスキップ
        if (line.empty() || line[0] == '#' || line[0] == ';' || line[0] == '[') continue;

        std::istringstream is_line(line);
        std::string key;
        if (std::getline(is_line, key, '=')) {
            std::string valueStr;
            if (std::getline(is_line, valueStr)) {
                try {
                    configMap[key] = std::stof(valueStr);
                }
                catch (...) {
                    DBG_ERROR("[IniParser] Invalid float value for key: " << key);
                }
            }
        }
    }
    return configMap;
}

bool IniParser::SaveFloatMap(const std::string& filePath, const std::unordered_map<std::string, float>& data) {
    std::ofstream file(filePath);
    if (!file.is_open()) {
        DBG_ERROR("[IniParser] CRITICAL: Failed to write to config file: " << filePath);
        return false;
    }

    file << "[CameraConfig]\n";
    for (const auto& pair : data) {
        file << pair.first << "=" << pair.second << "\n";
    }
    return true;
}