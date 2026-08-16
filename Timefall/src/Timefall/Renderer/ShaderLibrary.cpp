#include "tfpch.h"
#include "ShaderLibrary.h"

namespace Timefall
{
	static std::unordered_map<std::string, Ref<Shader>> s_Shaders;

	static std::string KeyFor(const std::filesystem::path& path)
	{
		std::error_code ec;
		// Weakly-canonical so a not-yet-existing path still normalises instead of throwing
		const std::filesystem::path canonical = std::filesystem::weakly_canonical(path, ec);
		return (ec ? path : canonical).string();
	}

	Ref<Shader> ShaderLibrary::Load(const std::filesystem::path& path)
	{
		const std::string key = KeyFor(path);
		if (s_Shaders.contains(key))
			return s_Shaders[key];

		Ref<Shader> shader = Shader::Create(path);
		s_Shaders.emplace(key, shader);
		return shader;
	}

	void ShaderLibrary::ForEach(const std::function<void(const Ref<Shader>&)>& fn)
	{
		for (const auto& [key, shader] : s_Shaders)
			fn(shader);
	}

	void ShaderLibrary::Shutdown()
	{
		ShutdownHotReload();
		s_Shaders.clear();
	}

	void ShaderLibrary::EnableHotReload(const std::filesystem::path& directory) {}
	void ShaderLibrary::ShutdownHotReload() {}
}
