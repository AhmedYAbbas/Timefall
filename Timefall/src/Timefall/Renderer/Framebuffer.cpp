#include "tfpch.h"
#include "Framebuffer.h"

namespace Timefall
{
	namespace
	{
		class StubFramebuffer : public Framebuffer
		{
		public:
			explicit StubFramebuffer(const FramebufferSpecification& spec)
				: m_Spec(spec)
			{}

			const FramebufferSpecification& GetSpecification() const override { return m_Spec; }

			uint32_t GetColorAttachmentRendererID(uint32_t) const override { return 0; }
			uint32_t GetDepthAttachmentRendererID() const override { return 0; }

			void BindColorAttachment(uint32_t, uint32_t) override {}
			void BindForSingleColorDraw(uint32_t) override {}
			void ClearColorAttachmentF(uint32_t, const glm::vec4&) override {}

			void Bind() override {}
			void Unbind() override {}

			void Resize(uint32_t width, uint32_t height) override
			{
				m_Spec.Width = width;
				m_Spec.Height = height;
			}

			int ReadPixel(uint32_t, int, int) override { return -1; }
			void ClearColorAttachment(uint32_t, int) override {}

		private:
			FramebufferSpecification m_Spec;
		};
	}

	Ref<Framebuffer> Framebuffer::Create(const FramebufferSpecification& spec)
	{
		return CreateRef<StubFramebuffer>(spec);
	}
}
