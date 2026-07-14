#pragma once

#include "Timefall/Renderer/ShadowMap.h"

namespace Timefall
{
	class OpenGLShadowMap : public ShadowMap
	{
	public:
		OpenGLShadowMap(uint32_t resolution, uint32_t layers);
		virtual ~OpenGLShadowMap();

		virtual void BeginRenderPass() override;
		virtual void BindLayer(uint32_t layer) override;
		virtual void EndRenderPass() override;

		virtual void BindForSampling(uint32_t slot) const override;

		virtual uint32_t GetDepthTextureID() const override { return m_DepthTexture; }
		virtual uint32_t GetResolution() const override { return m_Resolution; }
		virtual uint32_t GetLayerCount() const override { return m_Layers; }

	private:
		uint32_t m_RendererID = 0; // FBO
		uint32_t m_DepthTexture = 0; // GL_TEXTURE_2D_ARRAY
		uint32_t m_Resolution = 0;
		uint32_t m_Layers = 0;

		int m_PrevFramebuffer = 0;
		int m_PrevViewport[4] = {0, 0, 0, 0};
	};
}
