#include "tfpch.h"
#include "SceneImporter.h"

#include "Timefall/Project/Project.h"
#include "Timefall/Scene/SceneSerializer.h"

namespace Timefall
{
	Ref<Scene> SceneImporter::ImportScene(AssetHandle handle, const AssetMetadata& metadata)
	{
		TF_PROFILE_FUNCTION();

		return LoadScene(Project::GetAssetDirectory() / metadata.FilePath);
	}

	Ref<Scene> SceneImporter::LoadScene(const std::filesystem::path& filePath)
	{
		TF_PROFILE_FUNCTION();

		Ref<Scene> scene = CreateRef<Scene>();
		SceneSerializer serializer(scene);
		serializer.DeserializeText(filePath);
		return scene;
	}

	void SceneImporter::SaveScene(const Ref<Scene>& scene, const std::filesystem::path& filePath)
	{
		SceneSerializer serializer(scene);
		serializer.SerializeText(Project::GetAssetDirectory() / filePath);
	}
}