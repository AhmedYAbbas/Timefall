#include "tfpch.h"
#include "EditorAssetManager.h"
#include "AssetImporter.h"

#include "Timefall/Project/Project.h"

#include <fstream>
#include <yaml-cpp/yaml.h>

namespace Timefall
{
	static std::unordered_map<std::filesystem::path, AssetType> s_AssetExtensionsMap = {{".timefall", AssetType::Scene},
		{".png", AssetType::Texture2D}, {".jpg", AssetType::Texture2D}, {".jpeg", AssetType::Texture2D}, {".hdr", AssetType::Texture2D},
		{".tfmat", AssetType::Material}, {".obj", AssetType::Mesh}, {".fbx", AssetType::Mesh}, {".gltf", AssetType::Mesh},
		{".glb", AssetType::Mesh}};

	static AssetType GetAssetTypeFromFileExtension(const std::filesystem::path& extension)
	{
		if (!s_AssetExtensionsMap.contains(extension))
		{
			TF_CORE_WARN("Unknown asset type for extension: {0}", extension.string());
			return AssetType::None;
		}

		return s_AssetExtensionsMap.at(extension);
	}

	Ref<Asset> EditorAssetManager::GetAsset(AssetHandle handle)
	{
		if (!IsAssetHandleValid(handle))
		{
			TF_CORE_ERROR("Invalid asset handle: {0}", (uint64_t)handle);
			return nullptr;
		}

		Ref<Asset> asset;
		if (IsAssetLoaded(handle))
		{
			asset = m_LoadedAssets.at(handle);
		}
		else
		{
			const AssetMetadata& metadata = GetMetadata(handle);
			asset = AssetImporter::ImportAsset(handle, metadata);
			if (!asset)
				TF_CORE_ERROR("EditorAssetManager::GetAsset - Failed to import asset: {0}", metadata.FilePath.string());
			m_LoadedAssets[handle] = asset;
		}

		return asset;
	}

	bool EditorAssetManager::IsAssetHandleValid(AssetHandle handle) const
	{
		return (uint64_t)handle != 0 && m_AssetRegistry.contains(handle);
	}

	bool EditorAssetManager::IsAssetLoaded(AssetHandle handle) const
	{
		return m_LoadedAssets.contains(handle);
	}

	AssetType EditorAssetManager::GetAssetType(AssetHandle handle) const
	{
		if (!IsAssetHandleValid(handle))
			return AssetType::None;

		return m_AssetRegistry.at(handle).Type;
	}

	const std::filesystem::path& EditorAssetManager::GetFilePath(AssetHandle handle) const
	{
		return GetMetadata(handle).FilePath;
	}

	AssetHandle EditorAssetManager::ImportAsset(const std::filesystem::path& filePath)
	{
		AssetHandle handle;
		AssetMetadata metadata;
		metadata.FilePath = filePath;
		metadata.Type = GetAssetTypeFromFileExtension(filePath.extension());
		TF_CORE_ASSERT(metadata.Type != AssetType::None, "Unsupported asset type for file: {0}", filePath.string());
		Ref<Asset> asset = AssetImporter::ImportAsset(handle, metadata);
		if (!asset)
			return 0; // import failed — don't hand back an unregistered handle

		asset->Handle = handle;
		m_LoadedAssets[handle] = asset;
		m_AssetRegistry[handle] = metadata;
		SerializeAssetRegistry();
		return handle;
	}

	AssetHandle EditorAssetManager::ImportLoadedAsset(const std::filesystem::path& filePath, const Ref<Asset>& asset)
	{
		if (!asset)
			return 0;

		AssetHandle handle;
		AssetMetadata metadata;
		metadata.FilePath = filePath;
		metadata.Type = GetAssetTypeFromFileExtension(filePath.extension());
		TF_CORE_ASSERT(metadata.Type != AssetType::None, "Unsupported asset type for file: {0}", filePath.string());

		// Same registration as ImportAsset, but caches an instance produced elsewhere (e.g. during
		// model import) instead of re-reading the file. It IS disk-backed: on reload the asset is
		// re-imported from FilePath, unlike AddMemoryOnlyAsset.
		asset->Handle = handle;
		m_LoadedAssets[handle] = asset;
		m_AssetRegistry[handle] = metadata;
		SerializeAssetRegistry();
		return handle;
	}

	void EditorAssetManager::AddMemoryOnlyAsset(
		AssetHandle handle, const Ref<Asset>& asset, const std::filesystem::path& virtualPath, AssetType type)
	{
		asset->Handle = handle;
		m_LoadedAssets[handle] = asset;

		AssetMetadata metadata;
		metadata.FilePath = virtualPath;
		metadata.Type = type;
		metadata.MemoryOnly = true;
		m_AssetRegistry[handle] = metadata;
		// Not serialized: SerializeAssetRegistry skips MemoryOnly entries.
	}

	const AssetMetadata& EditorAssetManager::GetMetadata(AssetHandle handle) const
	{
		static AssetMetadata s_NullMetadata;
		if (!m_AssetRegistry.contains(handle))
			return s_NullMetadata;

		return m_AssetRegistry.at(handle);
	}

	void EditorAssetManager::SerializeAssetRegistry() const
	{
		auto path = Project::GetAssetRegistryPath();

		YAML::Emitter out;
		{
			out << YAML::BeginMap; // Root
			out << YAML::Key << "AssetRegistry" << YAML::Value;

			out << YAML::BeginSeq;
			for (const auto& [handle, metadata] : m_AssetRegistry)
			{
				if (metadata.MemoryOnly)
					continue;

				out << YAML::BeginMap;
				out << YAML::Key << "Handle" << YAML::Value << handle;
				out << YAML::Key << "FilePath" << YAML::Value << metadata.FilePath.generic_string();
				out << YAML::Key << "Type" << YAML::Value << AssetTypeToString(metadata.Type);
				out << YAML::EndMap;
			}
			out << YAML::EndSeq;

			out << YAML::EndMap; // Root
		}

		std::ofstream fout(path);
		fout << out.c_str();
	}

	bool EditorAssetManager::DeserializeAssetRegistry()
	{
		auto path = Project::GetAssetRegistryPath();

		YAML::Node data;
		try
		{
			data = YAML::LoadFile(path.string());
		}
		catch (YAML::ParserException e)
		{
			TF_CORE_ERROR("Failed to load project file");
			return false;
		}

		auto assetRegistryNode = data["AssetRegistry"];
		if (!assetRegistryNode)
			return false;

		for (const auto& node : assetRegistryNode)
		{
			AssetHandle handle = node["Handle"].as<uint64_t>();
			AssetMetadata& metadata = m_AssetRegistry[handle];

			metadata.FilePath = node["FilePath"].as<std::string>();
			metadata.Type = AssetTypeFromString(node["Type"].as<std::string>());
		}

		return true;
	}
}