#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include "CommonTypes.h"
#include "Utility/IniParser.h"

// =========================================================
// IniTable
// Camera::CONFIG_TABLE 方式（キー ⇔ 変数ポインタの表駆動）を一般化した汎用助手。
// float / int / Vector3 / Color を登録でき、内部ではすべて float 項目に展開して
// 既存の IniParser（KEY=値 の float マップ）へ読み書きを委譲する。
//   ・Vector3 → "_X/_Y/_Z" の 3 項目
//   ・Color   → "_R/_G/_B/_A" の 4 項目
// =========================================================
class IniTable {
public:
    void Add(const char* key, float* v) { m_entries.push_back({ key, v, nullptr }); }
    void Add(const char* key, int* v) { m_entries.push_back({ key, nullptr, v }); }

    void Add(const char* key, Vector3* v) {
        std::string k(key);
        Add((k + "_X").c_str(), &v->x);
        Add((k + "_Y").c_str(), &v->y);
        Add((k + "_Z").c_str(), &v->z);
    }
    void Add(const char* key, Color* c) {
        std::string k(key);
        Add((k + "_R").c_str(), &c->x);
        Add((k + "_G").c_str(), &c->y);
        Add((k + "_B").c_str(), &c->z);
        Add((k + "_A").c_str(), &c->w);
    }

    // INI から読み込み、登録済み変数へ反映（キー欠落はデフォルト値のまま）
    bool Load(const std::string& path) const {
        auto config = IniParser::LoadAsFloatMap(path);
        if (config.empty()) return false;   // ファイル無し：ハードコード既定値で続行

        for (const auto& e : m_entries) {
            auto it = config.find(e.key);
            if (it == config.end()) continue;
            if (e.f)      *e.f = it->second;
            else if (e.i) *e.i = (int)lroundf(it->second);  // int は四捨五入で復元
        }
        return true;
    }

    // 登録済み変数の現在値を INI へ書き出し
    bool Save(const std::string& path) const {
        std::unordered_map<std::string, float> config;
        for (const auto& e : m_entries) {
            config[e.key] = e.f ? *e.f : (float)(*e.i);
        }
        return IniParser::SaveFloatMap(path, config);
    }

private:
    struct Entry {
        std::string key;    // 展開後キーを所有する(サフィックス付きキーは動的生成のため)
        float* f = nullptr; // float 項目（Vector3/Color の各成分もここに展開される）
        int* i = nullptr;   // int 項目（float として保存し、読込時に丸める）
    };

    void Add(std::string key, float* v) { m_entries.push_back({ std::move(key), v, nullptr }); }

    std::vector<Entry> m_entries;
};