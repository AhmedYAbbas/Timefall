#pragma once

#include "Timefall/Renderer/Framebuffer.h"
#include <glm/glm.hpp>

namespace Timefall
{

	class TF_API OpenGLFramebuffer : public Framebuffer
	{
	public:
		OpenGLFramebuffer(const FramebufferSpecification& spec);
		virtual ~OpenGLFramebuffer();

		void Invalidate();

		virtual const FramebufferSpecification& GetSpecification() const override { return m_Specification; }
		virtual uint32_t GetColorAttachmentRendererID(uint32_t index) const override;
		virtual uint32_t GetDepthAttachmentRendererID() const override { return m_DepthAttachment; }

		virtual void BindColorAttachment(uint32_t index, uint32_t slot) override;
		virtual void BindForSingleColorDraw(uint32_t index) override;
		virtual void ClearColorAttachmentF(uint32_t index, const glm::vec4& color) override;

		virtual void Bind() override;
		virtual void Unbind() override;

		virtual void Resize(uint32_t width, uint32_t height) override;
		virtual int ReadPixel(uint32_t attachmentIndex, int x, int y) override;
		virtual void ClearColorAttachment(uint32_t attachmentIndex, int value) override;

	private:
		void DeleteOwnedAttachments();
		void SetDrawBuffers();

		uint32_t m_RendererID = 0;
		FramebufferSpecification m_Specification;

		std::vector<FramebufferTextureSpecification> m_ColorAttachmentSpecifications;
		FramebufferTextureSpecification m_DepthAttachmentSpecification;

		std::vector<uint32_t> m_ColorAttachments;
		uint32_t m_DepthAttachment = 0;
	};

}