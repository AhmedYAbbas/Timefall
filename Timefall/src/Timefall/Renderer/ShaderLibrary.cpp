#include "tfpch.h"
#include "ShaderLibrary.h"

#include "Timefall/Core/Application.h"
#include "Timefall/RHI/Pipeline.h"

#include "FileWatch.h"

#include <atomic>

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

	static Scope<filewatch::FileWatch<std::wstring>> s_Watcher;
	static std::atomic<uint64_t> s_EventGeneration = 0;
	static std::atomic<bool> s_ReloadScheduled = false;

	static void ScheduleReload();

	static void RunReload()
	{
		const uint64_t serviced = s_EventGeneration.load();

		uint32_t reloaded = 0;
		ShaderLibrary::ForEach([&reloaded](const Ref<Shader>& shader) { reloaded += shader->Reload() ? 1 : 0; });

		if (reloaded > 0)
		{
			const uint32_t rebuilt = RHI::GraphicsPipeline::RecreateAll();
			TF_CORE_INFO("Shader reload: {0} module(s) changed, {1} pipeline(s) rebuilt", reloaded, rebuilt);
		}

		s_ReloadScheduled = false;

		if (s_EventGeneration.load() != serviced)
			ScheduleReload();
	}

	static void ScheduleReload()
	{
		if (s_ReloadScheduled.exchange(true))
			return;

		Application::Get().SubmitToMainThread(RunReload);
	}

	static void OnShaderFileEvent(const std::wstring& path, const filewatch::Event change)
	{
		if (change != filewatch::Event::modified)
			return;
		if (std::filesystem::path(path).extension() != ".slang")
			return;

		s_EventGeneration.fetch_add(1);
		ScheduleReload();
	}

	Ref<Shader> ShaderLibrary::Load(const std::filesystem::path& path)
	{
		const std::string key = KeyFor(path);
		if (const auto it = s_Shaders.find(key); it != s_Shaders.end())
			return it->second;

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

	void ShaderLibrary::EnableHotReload(const std::filesystem::path& directory)
	{
		std::error_code ec;
		if (!std::filesystem::exists(directory, ec))
		{
			TF_CORE_WARN("Shader hot reload: '{0}' does not exist", directory.string());
			return;
		}

		s_Watcher = CreateScope<filewatch::FileWatch<std::wstring>>(directory.wstring(), OnShaderFileEvent);
		TF_CORE_INFO("Shader hot reload watching '{0}'", directory.string());
	}

	void ShaderLibrary::ShutdownHotReload()
	{
		s_Watcher.reset();
	}
}
