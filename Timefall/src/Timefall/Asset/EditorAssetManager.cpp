#include "tfpch.h"
#include "EditorAssetManager.h"
#include "AssetImporter.h"

#include "Timefall/Project/Project.h"

#include <fstream>
#include <yaml-cpp/yaml.h>

namespace Timefall
{
	Ref<Asset> EditorAssetManager::GetAsset(AssetHandle handle) const
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
			{
				TF_CORE_ERROR("EditorAssetManager::GetAsset - Failed to import asset: {0}", metadata.FilePath.string());
			}
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

	void EditorAssetManager::ImportAsset(const std::filesystem::path& filePath)
	{
		AssetHandle handle;
		AssetMetadata metadata;
		metadata.FilePath = filePath;
		metadata.Type = AssetType::Texture2D; // TODO: determine type based on file extension
		Ref<Asset> asset = AssetImporter::ImportAsset(handle, metadata);
		asset->Handle = handle;
		if (asset)
		{
			m_LoadedAssets[handle] = asset;
			m_AssetRegistry[handle] = metadata;
			SerializeAssetRegistry();
		}
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
			AssetHandle	handle = node["Handle"].as<uint64_t>();
			AssetMetadata& metadata = m_AssetRegistry[handle];

			metadata.FilePath = node["FilePath"].as<std::string>();
			metadata.Type = AssetTypeFromString(node["Type"].as<std::string>());
		}

		return true;
	}
}