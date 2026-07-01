#pragma once

#include <glm/glm.hpp>

namespace Timefall
{
	enum class FramebufferTextureFormat
	{
		None = 0,

		// Color
		RGBA8,
		RGBA16F,
		RED_INTEGER,

		// Depth/Stencil
		DEPTH24STENCIL8,

		// Default
		DEPTH = DEPTH24STENCIL8
	};

	struct FramebufferTextureSpecification
	{
		FramebufferTextureSpecification() = default;
		FramebufferTextureSpecification(FramebufferTextureFormat format)
			: TextureFormat(format)
		{
		}

		FramebufferTextureFormat TextureFormat = FramebufferTextureFormat::None;
		// Non-zero aliases an existing GL texture/renderbuffer instead of creating one.
		// The framebuffer does NOT own (delete) an aliased attachment.
		uint32_t ExternalRendererID = 0;
		// TODO: filtering/wraping
	};

	struct FramebufferAttachmentSpecification
	{
		FramebufferAttachmentSpecification() = default;
		FramebufferAttachmentSpecification(const std::initializer_list<FramebufferTextureSpecification>& attachments)
			: Attachments(attachments)
		{
		}

		std::vector<FramebufferTextureSpecification> Attachments;
	};

	struct FramebufferSpecification
	{
		uint32_t Width, Height;
		FramebufferAttachmentSpecification Attachments;
		uint32_t Samples = 1;

		bool SwapChainTarget = false;
	};

	class TF_API Framebuffer
	{
	public:
		virtual ~Framebuffer() = default;

		virtual const FramebufferSpecification& GetSpecification() const = 0;
		virtual uint32_t GetColorAttachmentRendererID(uint32_t index = 0) const = 0;
		virtual uint32_t GetDepthAttachmentRendererID() const = 0;

		virtual void BindColorAttachment(uint32_t index, uint32_t slot) = 0;
		virtual void BindForSingleColorDraw(uint32_t index) = 0;
		virtual void ClearColorAttachmentF(uint32_t index, const glm::vec4& color) = 0;

		virtual void Bind() = 0;
		virtual void Unbind() = 0;

		virtual void Resize(uint32_t width, uint32_t height) = 0;
		virtual int ReadPixel(uint32_t attachmentIndex, int x, int y) = 0;
		virtual void ClearColorAttachment(uint32_t attachmentIndex, int value) = 0;

		static Ref<Framebuffer> Create(const FramebufferSpecification& spec);
	};

}
