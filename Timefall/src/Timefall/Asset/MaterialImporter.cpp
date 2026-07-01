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

		// New (metallic-roughness) schema.
		if (auto c = node["BaseColor"])
			material->BaseColor = { c[0].as<float>(), c[1].as<float>(), c[2].as<float>() };
		if (auto m = node["Metallic"])
			material->Metallic = m.as<float>();
		if (auto r = node["Roughness"])
			material->Roughness = r.as<float>();
		if (auto n = node["NormalStrength"])
			material->NormalStrength = n.as<float>();
		if (auto m = node["BaseColorMap"])
			material->BaseColorMap = m.as<uint64_t>();
		if (auto m = node["NormalMap"])
			material->NormalMap = m.as<uint64_t>();
		if (auto m = node["MetallicMap"])
			material->MetallicMap = m.as<uint64_t>();
		if (auto m = node["RoughnessMap"])
			material->RoughnessMap = m.as<uint64_t>();
		if (auto m = node["AOMap"])
			material->AOMap = m.as<uint64_t>();
		if (auto c = node["Emissive"])
			material->Emissive = { c[0].as<float>(), c[1].as<float>(), c[2].as<float>() };
		if (auto e = node["EmissiveIntensity"])
			material->EmissiveIntensity = e.as<float>();
		if (auto m = node["EmissiveMap"])
			material->EmissiveMap = m.as<uint64_t>();

		// Back-compat: migrate legacy Blinn-Phong .tfmat files.
		if (auto c = node["DiffuseColor"]; c && !node["BaseColor"])
			material->BaseColor = { c[0].as<float>(), c[1].as<float>(), c[2].as<float>() };
		if (auto m = node["DiffuseMap"]; m && !node["BaseColorMap"])
			material->BaseColorMap = m.as<uint64_t>();

		return material;
	}

	void MaterialImporter::Serialize(const std::filesystem::path& path, const Ref<Material>& material)
	{
		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Material" << YAML::Value << YAML::BeginMap;

		out << YAML::Key << "BaseColor" << YAML::Value << YAML::Flow
			<< YAML::BeginSeq << material->BaseColor.x << material->BaseColor.y << material->BaseColor.z << YAML::EndSeq;
		out << YAML::Key << "Metallic" << YAML::Value << material->Metallic;
		out << YAML::Key << "Roughness" << YAML::Value << material->Roughness;
		out << YAML::Key << "NormalStrength" << YAML::Value << material->NormalStrength;
		out << YAML::Key << "BaseColorMap" << YAML::Value << (uint64_t)material->BaseColorMap;
		out << YAML::Key << "NormalMap" << YAML::Value << (uint64_t)material->NormalMap;
		out << YAML::Key << "MetallicMap" << YAML::Value << (uint64_t)material->MetallicMap;
		out << YAML::Key << "RoughnessMap" << YAML::Value << (uint64_t)material->RoughnessMap;
		out << YAML::Key << "AOMap" << YAML::Value << (uint64_t)material->AOMap;
		out << YAML::Key << "Emissive" << YAML::Value << YAML::Flow
			<< YAML::BeginSeq << material->Emissive.x << material->Emissive.y << material->Emissive.z << YAML::EndSeq;
		out << YAML::Key << "EmissiveIntensity" << YAML::Value << material->EmissiveIntensity;
		out << YAML::Key << "EmissiveMap" << YAML::Value << (uint64_t)material->EmissiveMap;

		out << YAML::EndMap;
		out << YAML::EndMap;

		std::ofstream fout(path);
		fout << out.c_str();
	}
}
