#include "tfpch.h"
#include "Platform/OpenGL/OpenGLShadowMap.h"

#include <glad/glad.h>

namespace Timefall
{
	OpenGLShadowMap::OpenGLShadowMap(uint32_t resolution, uint32_t layers)
		: m_Resolution(resolution),
		  m_Layers(layers)
	{
		// Depth array texture: one layer per cascade. 32-bit float depth for precision.
		glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &m_DepthTexture);
		glTextureStorage3D(m_DepthTexture, 1, GL_DEPTH_COMPONENT32F, resolution, resolution, layers);

		// PCSS reads raw depth and does its own comparisons, so no GL_TEXTURE_COMPARE_MODE.
		glTextureParameteri(m_DepthTexture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTextureParameteri(m_DepthTexture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTextureParameteri(m_DepthTexture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTextureParameteri(m_DepthTexture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
		// Border depth = 1.0 (far) so samples outside the map read as "lit".
		float border[4] = {1.0f, 1.0f, 1.0f, 1.0f};
		glTextureParameterfv(m_DepthTexture, GL_TEXTURE_BORDER_COLOR, border);

		glCreateFramebuffers(1, &m_RendererID);
		// Depth-only: no color attachment.
		glNamedFramebufferDrawBuffer(m_RendererID, GL_NONE);
		glNamedFramebufferReadBuffer(m_RendererID, GL_NONE);
	}

	OpenGLShadowMap::~OpenGLShadowMap()
	{
		glDeleteTextures(1, &m_DepthTexture);
		glDeleteFramebuffers(1, &m_RendererID);
	}

	void OpenGLShadowMap::BeginRenderPass()
	{
		glGetIntegerv(GL_FRAMEBUFFER_BINDING, &m_PrevFramebuffer);
		glGetIntegerv(GL_VIEWPORT, m_PrevViewport);
		glBindFramebuffer(GL_FRAMEBUFFER, m_RendererID);
	}

	void OpenGLShadowMap::BindLayer(uint32_t layer)
	{
		glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, m_DepthTexture, 0, (GLint)layer);
		glViewport(0, 0, m_Resolution, m_Resolution);
		glClear(GL_DEPTH_BUFFER_BIT);
	}

	void OpenGLShadowMap::EndRenderPass()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)m_PrevFramebuffer);
		glViewport(m_PrevViewport[0], m_PrevViewport[1], m_PrevViewport[2], m_PrevViewport[3]);
	}

	void OpenGLShadowMap::BindForSampling(uint32_t slot) const
	{
		glBindTextureUnit(slot, m_DepthTexture);
	}
}
