#pragma once

#include "Timefall/Core/Core.h"
#include "Timefall/Renderer/Shader.h"

namespace Timefall
{
	class TF_API ShaderLibrary
	{
	public:
		static Ref<Shader> Load(const std::filesystem::path& path);
		static void ForEach(const std::function<void(const Ref<Shader>&)>& fn);
		static void Shutdown();

		static void EnableHotReload(const std::filesystem::path& directory);
		static void ShutdownHotReload();
	};
}
