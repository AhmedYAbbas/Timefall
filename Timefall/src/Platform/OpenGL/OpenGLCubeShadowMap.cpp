#include "tfpch.h"
#include "Platform/OpenGL/OpenGLCubeShadowMap.h"

#include <glad/glad.h>

namespace Timefall
{
	OpenGLCubeShadowMap::OpenGLCubeShadowMap(uint32_t resolution, uint32_t cubeCount)
		: m_Resolution(resolution),
		  m_CubeCount(cubeCount)
	{
		glCreateTextures(GL_TEXTURE_CUBE_MAP_ARRAY, 1, &m_DepthTexture);
		glTextureStorage3D(m_DepthTexture, 1, GL_DEPTH_COMPONENT32F, resolution, resolution, cubeCount * 6);

		glTextureParameteri(m_DepthTexture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTextureParameteri(m_DepthTexture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTextureParameteri(m_DepthTexture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTextureParameteri(m_DepthTexture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTextureParameteri(m_DepthTexture, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

		glCreateFramebuffers(1, &m_RendererID);
		glNamedFramebufferDrawBuffer(m_RendererID, GL_NONE);
		glNamedFramebufferReadBuffer(m_RendererID, GL_NONE);
	}

	OpenGLCubeShadowMap::~OpenGLCubeShadowMap()
	{
		glDeleteTextures(1, &m_DepthTexture);
		glDeleteFramebuffers(1, &m_RendererID);
	}

	void OpenGLCubeShadowMap::BeginRenderPass()
	{
		glGetIntegerv(GL_FRAMEBUFFER_BINDING, &m_PrevFramebuffer);
		glGetIntegerv(GL_VIEWPORT, m_PrevViewport);
		glBindFramebuffer(GL_FRAMEBUFFER, m_RendererID);
	}

	void OpenGLCubeShadowMap::BindFace(uint32_t cubeIndex, uint32_t face)
	{
		glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, m_DepthTexture, 0, (GLint)(cubeIndex * 6 + face));
		glViewport(0, 0, m_Resolution, m_Resolution);
		glClear(GL_DEPTH_BUFFER_BIT);
	}

	void OpenGLCubeShadowMap::EndRenderPass()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)m_PrevFramebuffer);
		glViewport(m_PrevViewport[0], m_PrevViewport[1], m_PrevViewport[2], m_PrevViewport[3]);
	}

	void OpenGLCubeShadowMap::BindForSampling(uint32_t slot) const
	{
		glBindTextureUnit(slot, m_DepthTexture);
	}
}
