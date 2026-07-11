#pragma once

#include <string>
#include <functional>
#include <memory>
#include "MeshManager.h"
#include "CStaticMesh.h"
#include "CStaticMeshRenderer.h"

namespace ModelRegistry {

	inline CStaticMeshRenderer* RegisterModel(
		const std::string& name,
		const std::string& objPath,
		const std::string& texDir,
		const std::function<void(CStaticMeshRenderer&)>& setup = nullptr)
	{
		// 登録済みならロードせず既存を返す（多重ディスクIO防止）
		if (MeshManager::ContainsRenderer(name)) {
			return MeshManager::GetRenderer<CStaticMeshRenderer>(name);
		}

		auto mesh = std::make_unique<CStaticMesh>();
		mesh->Load(objPath, texDir);
		auto renderer = std::make_unique<CStaticMeshRenderer>();
		renderer->Init(*mesh);

		if (setup) setup(*renderer);

		CStaticMeshRenderer* raw = renderer.get();
		MeshManager::RegisterMesh<CStaticMesh>(name, std::move(mesh));
		MeshManager::RegisterMeshRenderer<CStaticMeshRenderer>(name, std::move(renderer));
		return raw;
	}
}