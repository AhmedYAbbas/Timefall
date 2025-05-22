#include "tfpch.h"

#include "Platform/OpenGL/OpenGLTexture.h"

#include <stb_image.h>
#include <glad/glad.h>

namespace Timefall
{
	OpenGLTexture2D::OpenGLTexture2D(uint32_t width, uint32_t height)
		: m_Width(width), m_Height(height)
	{
		TF_PROFILE_FUNCTION();

		m_InternalFormat = GL_RGBA8;
		m_DataFormat = GL_RGBA;

		glCreateTextures(GL_TEXTURE_2D, 1, &m_RendererID);
		glTextureStorage2D(m_RendererID, 1, m_InternalFormat, m_Width, m_Height);

		glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(m_RendererID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_T, GL_REPEAT);
	}

	OpenGLTexture2D::OpenGLTexture2D(const std::filesystem::path& path)
		: m_Path(path)
	{
		TF_PROFILE_FUNCTION();

		int width, height, channels;
		stbi_uc* data = nullptr;
		stbi_set_flip_vertically_on_load(1);
		{
			TF_PROFILE_SCOPE("stbi_load() - OpenGLTexture2D::OpenGLTexture2D(const std::string& path)");
			data = stbi_load(path.string().c_str(), &width, &height, &channels, 0);
		}
		TF_CORE_ASSERT(data, "Failed to load image!");
		m_Width = width;
		m_Height = height;

		GLenum internalFormat = 0, dataFormat = 0;
		if (channels == 4)
		{
			internalFormat = GL_RGBA8;
			dataFormat = GL_RGBA;
		}
		else if (channels == 3)
		{
			internalFormat = GL_RGB8;
			dataFormat = GL_RGB;
		}

		m_InternalFormat = internalFormat;
		m_DataFormat = dataFormat;

		TF_CORE_ASSERT(internalFormat & dataFormat, "Texture format is not supported!");

		glCreateTextures(GL_TEXTURE_2D, 1, &m_RendererID);
		glTextureStorage2D(m_RendererID, 1, internalFormat, m_Width, m_Height);

		glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(m_RendererID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_T, GL_REPEAT);

		glTextureSubImage2D(m_RendererID, 0, 0, 0, m_Width, m_Height, dataFormat, GL_UNSIGNED_BYTE, data);

		stbi_image_free(data);
	}

	OpenGLTexture2D::~OpenGLTexture2D()
	{
		TF_PROFILE_FUNCTION();

		glDeleteTextures(1, &m_RendererID);
	}

	std::vector<uint8_t> OpenGLTexture2D::GetData() const
	{
		uint32_t bpp = m_DataFormat == GL_RGBA ? 4 : 3;
		uint32_t size = m_Width * m_Height * bpp;

		std::vector<uint8_t> pixels(size);
		glGetTextureImage(m_RendererID, 0, m_DataFormat, GL_UNSIGNED_BYTE, size, pixels.data());

		TF_CORE_ASSERT(pixels.size() > 0, "Failed to get texture data!");
		return pixels;
	}

	void OpenGLTexture2D::SetData(void* data, uint32_t size)
	{
		TF_PROFILE_FUNCTION();

		uint32_t bpp = m_DataFormat == GL_RGBA ? 4 : 3;
		TF_CORE_ASSERT(size == m_Width * m_Height * bpp, "Data must be entire texture");
		glTextureSubImage2D(m_RendererID, 0, 0, 0, m_Width, m_Height, m_DataFormat, GL_UNSIGNED_BYTE, data);
	}

	void OpenGLTexture2D::SetData(const std::vector<uint8_t>& data, uint32_t dataFormat)
	{
		TF_PROFILE_FUNCTION();

		uint32_t bpp = dataFormat == GL_RGBA ? 4 : 3;
		TF_CORE_ASSERT(data.size() == m_Width * m_Height * bpp, "Data must be entire texture");
		m_DataFormat = dataFormat;
		glTextureSubImage2D(m_RendererID, 0, 0, 0, m_Width, m_Height, m_DataFormat, GL_UNSIGNED_BYTE, data.data());
	}

	void OpenGLTexture2D::Bind(int slot) const
	{
		TF_PROFILE_FUNCTION();

		glBindTextureUnit(slot, m_RendererID);
	}
}