#pragma once

#include "Timefall/Renderer/ShaderReflection.h"

#include <string>
#include <filesystem>
#include <span>

namespace Timefall
{
	class TF_API Shader
	{
	public:
		static Ref<Shader> Create(const std::filesystem::path& path);

		const std::filesystem::path& GetPath() const { return m_Path; }
		const std::string& GetName() const { return m_Name; }
		const ShaderReflection& GetReflection() const { return m_Reflection; }

		bool HasStage(ShaderStage stage) const;
		std::span<const uint32_t> GetSpirv(ShaderStage stage) const;
		const std::string& GetEntryPointName(ShaderStage stage) const;

		uint32_t GetRevision() const { return m_Revision; }

		std::span<const std::filesystem::path> GetDependencies() const { return m_Dependencies; }

		bool IsValid() const { return !m_EntryPoints.empty(); }

		bool Reload();

	private:
		explicit Shader(const std::filesystem::path& path);

		bool Compile();

	private:
		struct Entry
		{
			std::string Name;
			ShaderStage Stage = ShaderStage::Vertex;
			std::vector<uint32_t> Spirv;
		};

		static uint64_t ModuleHash(std::span<const Entry> entries);

		std::filesystem::path m_Path;
		std::string m_Name;
		std::vector<Entry> m_EntryPoints;
		ShaderReflection m_Reflection;
		std::vector<std::filesystem::path> m_Dependencies;
		uint64_t m_ModuleHash = 0;
		uint32_t m_Revision = 0;
	};
}
