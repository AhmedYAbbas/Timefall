#pragma once

#include "Timefall/Core/Core.h"
#include "Timefall/RHI/RenderTarget.h"
#include "Timefall/Renderer/Texture.h"

#include <filesystem>

namespace Timefall
{
	// Bakes an equirectangular HDR into the three IBL cubemaps. Built once per unique
	// environment texture and cached by the renderer.
	class TF_API Environment
	{
	public:
		static Ref<Environment> Create(const Ref<Texture2D>& equirect, const std::filesystem::path& sourcePath);
		static void ReleaseResources();

		const Ref<RHI::Texture>& GetSkyboxMap() const;
		const Ref<RHI::Texture>& GetIrradianceMap() const;
		const Ref<RHI::Texture>& GetPrefilterMap() const;

		bool IsValid() const;

	private:
		Environment() = default;

		Ref<RHI::RenderTarget> m_Skybox;
		Ref<RHI::RenderTarget> m_Irradiance;
		Ref<RHI::RenderTarget> m_Prefilter;
	};
}
