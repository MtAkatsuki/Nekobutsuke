#pragma once
#include <string>
#include <memory>
#include <unordered_map>
#include <functional>
#include "IScene.h"

// =========================================================
// SceneClassFactory クラス (Singleton)
// 文字列名から IScene 派生クラスのインスタンスを動的に生成するためのファクトリ。
// REGISTER_CLASS マクロによって各シーンが自動登録される仕組みを提供する。
// =========================================================
class SceneClassFactory {
public:
    using SceneCreatorFunc = std::function<std::unique_ptr<IScene>()>;

    static SceneClassFactory& getInstance() {
        static SceneClassFactory instance;
        return instance;
    }

    void registerClass(const std::string& name, SceneCreatorFunc func) {
        m_registry[name] = func;
    }

    std::unique_ptr<IScene> create(const std::string& name) {
        auto it = m_registry.find(name);
        if (it != m_registry.end()) {
            return it->second();
        }
        return nullptr;
    }

private:
    std::unordered_map<std::string, SceneCreatorFunc> m_registry;
};

// =========================================================
// 自動登録マクロ
// 各シーンの .cpp 末尾に記述することで、起動時に静的にファクトリへ登録される。
// =========================================================
#define REGISTER_CLASS(CLASSNAME) \
    namespace { \
        struct CLASSNAME##Registrar { \
            CLASSNAME##Registrar() { \
                SceneClassFactory::getInstance().registerClass(#CLASSNAME, []() { \
                    return std::make_unique<CLASSNAME>(); \
                }); \
            } \
        }; \
        static CLASSNAME##Registrar global_##CLASSNAME##_registrar; \
    }