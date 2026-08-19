#include "tfpch.h"

#include "Timefall/Renderer/Shader.h"
#include "Timefall/Renderer/ShaderCache.h"

namespace Timefall
{
	static const std::string s_Empty;

	uint64_t Shader::ModuleHash(std::span<const Entry> entries)
	{
		uint64_t hash = 14695981039346656037ull;
		const auto mix = [&hash](const void* data, size_t size) {
			const auto* bytes = (const uint8_t*)data;
			for (size_t i = 0; i < size; i++)
			{
				hash ^= bytes[i];
				hash *= 1099511628211ull;
			}
		};

		for (const auto& entry : entries)
		{
			mix(entry.Name.data(), entry.Name.size());
			mix(&entry.Stage, sizeof(entry.Stage));
			mix(entry.Spirv.data(), entry.Spirv.size() * sizeof(uint32_t));
		}

		return hash;
	}

	Shader::Shader(const std::filesystem::path& path)
		: m_Path(path),
		  m_Name(path.stem().string())
	{}

	Ref<Shader> Shader::Create(const std::filesystem::path& path)
	{
		Ref<Shader> shader(new Shader(path));
		if (!shader->Compile())
			TF_CORE_ERROR("Shader '{0}' failed its initial compile; it will render nothing until fixed", shader->m_Name);

		return shader;
	}

	bool Shader::Compile()
	{
		auto compiled = ShaderCache::GetOrCompile(m_Path);
		if (!compiled)
		{
			TF_CORE_ERROR("{0}({1}): {2}", compiled.error().File, compiled.error().Line, compiled.error().Message);
			return false;
		}

		m_EntryPoints.clear();
		m_EntryPoints.reserve(compiled->EntryPoints.size());
		for (auto& entry : compiled->EntryPoints)
			m_EntryPoints.push_back({std::move(entry.Name), entry.Stage, std::move(entry.SpirV)});

		m_Reflection = std::move(compiled->Reflection);
		m_Dependencies = std::move(compiled->Dependencies);

		const uint64_t hash = ModuleHash(m_EntryPoints);
		if (hash != m_ModuleHash)
		{
			m_ModuleHash = hash;
			++m_Revision;
		}

		return true;
	}

	bool Shader::Reload()
	{
		TF_PROFILE_FUNCTION();

		const uint32_t previous = m_Revision;
		if (!Compile())
		{
			TF_CORE_ERROR("Shader '{0}' reload failed; keeping the last good module", m_Name);
			return false;
		}

		if (m_Revision == previous)
			return false;

		TF_CORE_INFO("Shader '{0}' reloaded (revision {1})", m_Name, m_Revision);
		return true;
	}

	bool Shader::HasStage(ShaderStage stage) const
	{
		return std::ranges::any_of(m_EntryPoints, [stage](const Entry& e) { return e.Stage == stage; });
	}

	std::span<const uint32_t> Shader::GetSpirv(ShaderStage stage) const
	{
		for (const auto& entry : m_EntryPoints)
			if (entry.Stage == stage)
				return entry.Spirv;

		return {};
	}

	const std::string& Shader::GetEntryPointName(ShaderStage stage) const
	{
		for (const auto& entry : m_EntryPoints)
			if (entry.Stage == stage)
				return entry.Name;

		return s_Empty;
	}
}
