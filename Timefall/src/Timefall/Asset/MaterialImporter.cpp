#include "tfpch.h"
#include "MaterialImporter.h"

#include "Timefall/Project/Project.h"

#include <fstream>
#include <yaml-cpp/yaml.h>

namespace Timefall
{
	Ref<Material> MaterialImporter::ImportMaterial(AssetHandle handle, const AssetMetadata& metadata)
	{
		return LoadMaterial(Project::GetAssetDirectory() / metadata.FilePath);
	}

	Ref<Material> MaterialImporter::LoadMaterial(const std::filesystem::path& path)
	{
		YAML::Node data;
		try
		{
			data = YAML::LoadFile(path.string());
		}
		catch (YAML::ParserException& e)
		{
			TF_CORE_ERROR("MaterialImporter::LoadMaterial - Failed to parse {0}: {1}", path.string(), e.what());
			return nullptr;
		}

		auto node = data["Material"];
		if (!node)
			return nullptr;

		Ref<Material> material = CreateRef<Material>();

		if (auto c = node["DiffuseColor"])
			material->DiffuseColor = { c[0].as<float>(), c[1].as<float>(), c[2].as<float>() };
		if (auto c = node["SpecularColor"])
			material->SpecularColor = { c[0].as<float>(), c[1].as<float>(), c[2].as<float>() };
		if (auto s = node["Shininess"])
			material->Shininess = s.as<float>();
		if (auto m = node["DiffuseMap"])
			material->DiffuseMap = m.as<uint64_t>();
		if (auto m = node["SpecularMap"])
			material->SpecularMap = m.as<uint64_t>();
		if (auto m = node["NormalMap"])
			material->NormalMap = m.as<uint64_t>();

		return material;
	}

	void MaterialImporter::Serialize(const std::filesystem::path& path, const Ref<Material>& material)
	{
		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Material" << YAML::Value << YAML::BeginMap;

		out << YAML::Key << "DiffuseColor" << YAML::Value << YAML::Flow
			<< YAML::BeginSeq << material->DiffuseColor.x << material->DiffuseColor.y << material->DiffuseColor.z << YAML::EndSeq;
		out << YAML::Key << "SpecularColor" << YAML::Value << YAML::Flow
			<< YAML::BeginSeq << material->SpecularColor.x << material->SpecularColor.y << material->SpecularColor.z << YAML::EndSeq;
		out << YAML::Key << "Shininess" << YAML::Value << material->Shininess;
		out << YAML::Key << "DiffuseMap" << YAML::Value << (uint64_t)material->DiffuseMap;
		out << YAML::Key << "SpecularMap" << YAML::Value << (uint64_t)material->SpecularMap;
		out << YAML::Key << "NormalMap" << YAML::Value << (uint64_t)material->NormalMap;

		out << YAML::EndMap;
		out << YAML::EndMap;

		std::ofstream fout(path);
		fout << out.c_str();
	}
}
