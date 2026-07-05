#pragma once

#include "Timefall/Renderer/TextureCube.h"

namespace Timefall
{
	class TF_API OpenGLTextureCube : public TextureCube
	{
	public:
		OpenGLTextureCube(uint32_t size, uint32_t mipLevels);
		virtual ~OpenGLTextureCube();

		virtual void BindForSampling(uint32_t slot) const override;

		virtual uint32_t GetRendererID() const override { return m_RendererID; }
		virtual uint32_t GetSize() const override { return m_Size; }
		virtual uint32_t GetMipLevels() const override { return m_MipLevels; }

	private:
		uint32_t m_RendererID = 0;
		uint32_t m_Size = 0;
		uint32_t m_MipLevels = 1;
	};
}
