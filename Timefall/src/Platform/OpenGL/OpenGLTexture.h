#pragma once

#include "Timefall/Renderer/Texture.h"

namespace Timefall
{
	class OpenGLTexture2D : public Texture2D
	{
	public:
		OpenGLTexture2D(uint32_t width, uint32_t height);
		OpenGLTexture2D(const std::string& path);
		virtual ~OpenGLTexture2D();

		virtual uint32_t GetWidth() const override { return m_Width; };
		virtual uint32_t GetHeight() const override { return m_Height; };
		virtual uint32_t GetRendererID() const override { return m_RendererID; };

		virtual uint32_t GetInternalFormat() const override { return m_InternalFormat; }
		virtual uint32_t GetDataFormat() const override { return m_DataFormat; }

		virtual std::vector<uint8_t> GetData() const override;

		virtual void SetData(void* data, uint32_t size) override;
		virtual void SetData(const std::vector<uint8_t>& data, uint32_t dataFormat) override;

		virtual void Bind(int slot = 0) const override;

		virtual bool operator==(const Texture& other) const override
		{
			return m_RendererID == ((OpenGLTexture2D&)other).m_RendererID;
		}

	private:
		std::string m_Path;
		uint32_t m_Width, m_Height;
		uint32_t m_RendererID;
		uint32_t m_InternalFormat, m_DataFormat;
	};
}