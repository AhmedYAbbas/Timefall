#include "tfpch.h"

#include "Timefall/Renderer/Texture.h"

namespace Timefall
{
	namespace
	{
		class StubTexture2D : public Texture2D
		{
		public:
			explicit StubTexture2D(const TextureSpecification& spec)
				: m_Spec(spec)
			{}

			const TextureSpecification& GetSpecification() const override { return m_Spec; }

			uint32_t GetWidth() const override { return m_Spec.Width; }
			uint32_t GetHeight() const override { return m_Spec.Height; }
			uint32_t GetRendererID() const override { return 0; }

			uint32_t GetInternalFormat() const override { return 0; }
			uint32_t GetDataFormat() const override { return 0; }

			std::vector<uint8_t> GetData() const override { return {}; }
			void SetData(Buffer) override {}
			void SetData(const std::vector<uint8_t>&, uint32_t) override {}

			void Bind(int) const override {}
			void BindAsSRGB(int) const override {}

			bool operator==(const Texture& other) const override { return this == &other; }

		private:
			TextureSpecification m_Spec;
		};
	}

	Ref<Texture2D> Texture2D::Create(const TextureSpecification& spec, Buffer)
	{
		return CreateRef<StubTexture2D>(spec);
	}
}
