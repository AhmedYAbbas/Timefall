#include "tfpch.h"
#include "Timefall/Renderer/TextureCube.h"

namespace Timefall
{
	namespace
	{
		class StubTextureCube : public TextureCube
		{
		public:
			StubTextureCube(uint32_t size, uint32_t mipLevels)
				: m_Size(size),
				  m_MipLevels(mipLevels)
			{}

			void BindForSampling(uint32_t) const override {}

			uint32_t GetRendererID() const override { return 0; }
			uint32_t GetSize() const override { return m_Size; }
			uint32_t GetMipLevels() const override { return m_MipLevels; }

		private:
			uint32_t m_Size;
			uint32_t m_MipLevels;
		};
	}

	Ref<TextureCube> TextureCube::Create(uint32_t size, uint32_t mipLevels)
	{
		return CreateRef<StubTextureCube>(size, mipLevels);
	}
}
