#pragma once
#include <memory>
#include <unordered_map>
#include <string>
#include <array>
#include "CMesh.h"
#include "CMeshRenderer.h"
#include "CShader.h"
#include "noncopyable.h"

class MeshManager : NonCopyable{

	static inline std::unordered_map<std::string, std::unique_ptr<CMesh>> m_meshcontainer{};
	static inline std::unordered_map<std::string, std::unique_ptr<CMeshRenderer>> m_renderercontainer{};
	static inline std::unordered_map<std::string, std::unique_ptr<CShader>> m_shadercontainer{};

public:
	template<class T>
	static bool RegisterMesh(std::string key, std::unique_ptr<T> meshdata){
		// 存在してる場合は何もしない
		if (m_meshcontainer.contains(key)) return false;

		// 存在していなければ登録
		m_meshcontainer[key] = std::move(meshdata);
		return true;
	}

	template<class T>
	static bool RegisterMeshRenderer(std::string key, std::unique_ptr<T> data) {
		// 存在してる場合は何もしない
		if (m_renderercontainer.contains(key)) return false;

		// 存在していなければ登録
		m_renderercontainer[key] = std::move(data);
		return true;
	}

	template<class T>
	static bool RegisterShader(std::string key, std::unique_ptr<T> data) {
		// 存在してる場合は何もしない
		if (m_shadercontainer.contains(key)) return false;

		// 存在していなければ登録
		m_shadercontainer[key] = std::move(data);
		return true;
	}

	template<class T>
	static T* getMesh(std::string key) {
		return static_cast<T*>(m_meshcontainer[key].get());
	}
	template<class T>
	static T* GetShader(std::string key) {
		return static_cast<T*>(m_shadercontainer[key].get());
	}
	template<class T>
	static T* GetRenderer(std::string key) {
		return static_cast<T*>(m_renderercontainer[key].get());
	}

	// 登録済み判定。
	static bool ContainsRenderer(const std::string& key) {
		return m_renderercontainer.contains(key);
	}
};
