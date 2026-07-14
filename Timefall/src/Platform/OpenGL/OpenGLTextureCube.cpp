#include "tfpch.h"
#include "Platform/OpenGL/OpenGLTextureCube.h"

#include <glad/glad.h>

namespace Timefall
{
	Ref<TextureCube> TextureCube::Create(uint32_t size, uint32_t mipLevels)
	{
		return CreateRef<OpenGLTextureCube>(size, mipLevels);
	}

	OpenGLTextureCube::OpenGLTextureCube(uint32_t size, uint32_t mipLevels)
		: m_Size(size),
		  m_MipLevels(mipLevels == 0 ? 1 : mipLevels)
	{
		glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &m_RendererID);
		glTextureStorage2D(m_RendererID, (GLsizei)m_MipLevels, GL_RGBA16F, (GLsizei)size, (GLsizei)size);

		glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, m_MipLevels > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
		glTextureParameteri(m_RendererID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	}

	OpenGLTextureCube::~OpenGLTextureCube()
	{
		glDeleteTextures(1, &m_RendererID);
	}

	void OpenGLTextureCube::BindForSampling(uint32_t slot) const
	{
		glBindTextureUnit(slot, m_RendererID);
	}
}
