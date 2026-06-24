#include "tfpch.h"

#include "Platform/OpenGL/OpenGLTexture.h"

#include <glad/glad.h>

#include <cmath>

namespace Timefall
{
	namespace Utils
	{
		static GLenum TimefallImageFormatToGLDataFormat(ImageFormat format)
		{
			switch (format)
			{
				case ImageFormat::R8:		return GL_RED;
				case ImageFormat::RGB8:		return GL_RGB;
				case ImageFormat::RGBA8:	return GL_RGBA;
				case ImageFormat::RGB32F:	return GL_RGB32F;
			}

			TF_CORE_ASSERT(false, "Unknown ImageFormat!");
			return 0;
		}

		static GLenum TimefallImageFormatToGLInternalFormat(ImageFormat format)
		{
			switch (format)
			{
				case ImageFormat::R8:		return GL_R8;
				case ImageFormat::RGB8:		return GL_RGB8;
				case ImageFormat::RGBA8:	return GL_RGBA8;
				case ImageFormat::RGB32F:	return GL_RGB32F;
			}

			TF_CORE_ASSERT(false, "Unknown ImageFormat!");
			return 0;
		}
	}

	OpenGLTexture2D::OpenGLTexture2D(const TextureSpecification& spec, Buffer data)
		: m_Specification(spec), m_Width(spec.Width), m_Height(spec.Height)
	{
		TF_PROFILE_FUNCTION();

		m_InternalFormat = Utils::TimefallImageFormatToGLInternalFormat(m_Specification.Format);
		m_DataFormat = Utils::TimefallImageFormatToGLDataFormat(m_Specification.Format);

		uint32_t mipLevels = m_Specification.GenerateMips
			? 1 + (uint32_t)std::floor(std::log2((float)std::max(m_Width, m_Height)))
			: 1;

		glCreateTextures(GL_TEXTURE_2D, 1, &m_RendererID);
		glTextureStorage2D(m_RendererID, mipLevels, m_InternalFormat, m_Width, m_Height);

		glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER,
			m_Specification.GenerateMips ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
		glTextureParameteri(m_RendererID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_T, GL_REPEAT);

		if (data)
			SetData(data);
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

	void OpenGLTexture2D::SetData(Buffer data)
	{
		TF_PROFILE_FUNCTION();

		uint32_t bpp = m_DataFormat == GL_RGBA ? 4 : 3;
		TF_CORE_ASSERT(data.Size == m_Width * m_Height * bpp, "Data must be entire texture");
		glTextureSubImage2D(m_RendererID, 0, 0, 0, m_Width, m_Height, m_DataFormat, GL_UNSIGNED_BYTE, data.Data);

		if (m_Specification.GenerateMips)
			glGenerateTextureMipmap(m_RendererID);
	}

	void OpenGLTexture2D::SetData(const std::vector<uint8_t>& data, uint32_t dataFormat)
	{
		TF_PROFILE_FUNCTION();

		uint32_t bpp = dataFormat == GL_RGBA ? 4 : 3;
		TF_CORE_ASSERT(data.size() == m_Width * m_Height * bpp, "Data must be entire texture");
		m_DataFormat = dataFormat;
		glTextureSubImage2D(m_RendererID, 0, 0, 0, m_Width, m_Height, m_DataFormat, GL_UNSIGNED_BYTE, data.data());

		if (m_Specification.GenerateMips)
			glGenerateTextureMipmap(m_RendererID);
	}

	void OpenGLTexture2D::Bind(int slot) const
	{
		TF_PROFILE_FUNCTION();

		glBindTextureUnit(slot, m_RendererID);
	}
}