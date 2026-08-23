#pragma once

#include "Timefall/Core/Core.h"
#include "Timefall/RHI/RenderTarget.h"
#include "Timefall/Renderer/Texture.h"

namespace Timefall
{
	// Bakes an equirectangular HDR into the three IBL cubemaps. Built once per unique
	// environment texture and cached by the renderer.
	class TF_API Environment
	{
	public:
		static Ref<Environment> Create(const Ref<Texture2D>& equirect);
		static void ReleaseResources();

		const Ref<RHI::Texture>& GetSkyboxMap() const;
		const Ref<RHI::Texture>& GetIrradianceMap() const;

		bool IsValid() const;

	private:
		Environment() = default;

		Ref<RHI::RenderTarget> m_Skybox;
		Ref<RHI::RenderTarget> m_Irradiance;
	};
}
