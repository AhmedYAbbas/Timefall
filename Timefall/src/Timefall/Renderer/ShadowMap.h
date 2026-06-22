#pragma once

#include "Timefall/Core/Core.h"

#include <cstdint>

namespace Timefall
{
	// Depth-only shadow target backed by a GL_TEXTURE_2D_ARRAY (one layer per cascade).
	// Built layered from the start; Phase A.0 uses a single layer. The render-pass calls
	// save and restore the previously bound framebuffer + viewport so the caller's scene
	// target is untouched.
	class TF_API ShadowMap
	{
	public:
		virtual ~ShadowMap() = default;

		// Save current FBO + viewport, then bind this shadow map's FBO.
		virtual void BeginRenderPass() = 0;
		// Attach `layer` as the depth target, set the viewport to the map resolution, clear depth.
		virtual void BindLayer(uint32_t layer) = 0;
		// Restore the framebuffer + viewport captured in BeginRenderPass.
		virtual void EndRenderPass() = 0;

		// Bind the depth array texture to a sampler unit for reading in the lit pass.
		virtual void BindForSampling(uint32_t slot) const = 0;

		virtual uint32_t GetDepthTextureID() const = 0;
		virtual uint32_t GetResolution() const = 0;
		virtual uint32_t GetLayerCount() const = 0;

		static Ref<ShadowMap> Create(uint32_t resolution, uint32_t layers);
	};
}
