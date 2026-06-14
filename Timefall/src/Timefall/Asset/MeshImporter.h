#pragma once

#include "Timefall/Core/Core.h"
#include "Timefall/Asset/AssetMetadata.h"
#include "Timefall/Renderer/Mesh.h"
#include "Timefall/Scene/Entity.h"

namespace Timefall
{
	class Scene;

	class TF_API MeshImporter
	{
	public:
		// Asset-pipeline (per load): geometry only.
		static Ref<MeshSource> ImportMesh(AssetHandle handle, const AssetMetadata& metadata);
		static Ref<MeshSource> LoadMesh(const std::filesystem::path& path);

		// Editor one-shot: register the mesh asset, generate .tfmat materials, spawn a parented
		// entity tree. Returns the spawned root entity.
		static Entity ImportModel(const Ref<Scene>& scene, const std::filesystem::path& modelPath);
	};
}
