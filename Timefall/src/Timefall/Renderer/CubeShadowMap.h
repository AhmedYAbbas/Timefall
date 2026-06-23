#pragma once

#include "Timefall/Core/Core.h"

#include <cstdint>

namespace Timefall
{
	// Depth cube-map array (one cube per caster). Stores linear distance-to-light.
	class TF_API CubeShadowMap
	{
	public:
		virtual ~CubeShadowMap() = default;

		virtual void BeginRenderPass() = 0;
		// Attach one face (cubeIndex*6 + face), set viewport, clear depth.
		virtual void BindFace(uint32_t cubeIndex, uint32_t face) = 0;
		virtual void EndRenderPass() = 0;

		virtual void BindForSampling(uint32_t slot) const = 0;

		virtual uint32_t GetResolution() const = 0;
		virtual uint32_t GetCubeCount() const = 0;

		static Ref<CubeShadowMap> Create(uint32_t resolution, uint32_t cubeCount);
	};
}
