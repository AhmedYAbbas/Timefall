#pragma once

#include "Timefall/Core/Core.h"
#include "Timefall/Renderer/Texture.h"
#include "Timefall/Renderer/TextureCube.h"

namespace Timefall
{
	// Bakes an equirectangular HDR into the three IBL cubemaps. Built once per unique
	// environment texture and cached by the renderer.
	class TF_API Environment
	{
	public:
		static Ref<Environment> Create(const Ref<Texture2D>& equirect);

		const Ref<RHI::Texture>& GetSkyboxMap() const;
		bool IsValid() const;

	private:
		Environment() = default;

		Ref<RHI::RenderTarget> m_Skybox;
	};
}
