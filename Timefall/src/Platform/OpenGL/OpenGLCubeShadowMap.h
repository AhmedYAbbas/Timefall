#pragma once

#include "Timefall/Renderer/CubeShadowMap.h"

namespace Timefall
{
	class OpenGLCubeShadowMap : public CubeShadowMap
	{
	public:
		OpenGLCubeShadowMap(uint32_t resolution, uint32_t cubeCount);
		virtual ~OpenGLCubeShadowMap();

		virtual void BeginRenderPass() override;
		virtual void BindFace(uint32_t cubeIndex, uint32_t face) override;
		virtual void EndRenderPass() override;

		virtual void BindForSampling(uint32_t slot) const override;

		virtual uint32_t GetResolution() const override { return m_Resolution; }
		virtual uint32_t GetCubeCount() const override { return m_CubeCount; }

	private:
		uint32_t m_RendererID = 0;     // FBO
		uint32_t m_DepthTexture = 0;   // GL_TEXTURE_CUBE_MAP_ARRAY
		uint32_t m_Resolution = 0;
		uint32_t m_CubeCount = 0;

		int m_PrevFramebuffer = 0;
		int m_PrevViewport[4] = { 0, 0, 0, 0 };
	};
}
