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
		virtual ~Environment() = default;

		virtual Ref<TextureCube> GetSkyboxMap() const = 0;
		virtual Ref<TextureCube> GetIrradianceMap() const = 0;
		virtual Ref<TextureCube> GetPrefilterMap() const = 0;
		virtual uint32_t GetPrefilterMipLevels() const = 0;

		static Ref<Environment> Create(const Ref<Texture2D>& equirect);
	};
}
