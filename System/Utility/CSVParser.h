#pragma once
#include <vector>
#include <string>

class CSVParser {
public:
    static std::vector<std::vector<std::string>> ReadAsGrid(const std::string& filePath);
};