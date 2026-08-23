#include "tfpch.h"

#include "Timefall/RHI/RenderTarget.h"

namespace Timefall::RHI
{
	static const Ref<Texture> s_NullTexture;

	struct RenderTarget::Impl
	{
		RenderTargetDesc Desc;
		std::array<Ref<Texture>, 8> Color{};
		Ref<Texture> Depth;

		bool BuildAttachments()
		{
			const char* base = Desc.DebugName ? Desc.DebugName : "RenderTarget";

			for (uint32_t i = 0; i < Desc.ColorCount; i++)
			{
				const std::string name = std::format("{}:Color{}", base, i);

				Color[i] = Texture::Create({.Width = Desc.Width,
					.Height = Desc.Height,
					.PixelFormat = Desc.ColorFormat[i],
					.MipLevels = Desc.MipLevels,
					.Dim = Desc.Dim,
					.ArrayLayers = Desc.ArrayLayers,
					.Usage = TextureUsage::Sampled | TextureUsage::ColorAttachment | Desc.ColorUsage[i],
					.DebugName = name.c_str()});

				if (!Color[i] || !Color[i]->IsValid())
				{
					TF_CORE_ERROR("RenderTarget '{0}' color attachment {1} ({2}x{3}) failed to allocate", base, i, Desc.Width, Desc.Height);
					return false;
				}
			}

			if (Desc.DepthFormat == Format::Undefined)
				return true;

			const std::string depthName = std::format("{}:Depth", base);

			Depth = Texture::Create({.Width = Desc.Width,
				.Height = Desc.Height,
				.PixelFormat = Desc.DepthFormat,
				.MipLevels = 1,
				.Usage = TextureUsage::Sampled | TextureUsage::DepthAttachment,
				.DebugName = depthName.c_str()});

			if (!Depth || !Depth->IsValid())
			{
				TF_CORE_ERROR("RenderTarget '{0}' depth attachment ({1}x{2}) failed to allocate", base, Desc.Width, Desc.Height);
				return false;
			}

			return true;
		}

		void DropAttachments()
		{
			Color.fill(nullptr);
			Depth.reset();
		}
	};

	Ref<RenderTarget> RenderTarget::Create(const RenderTargetDesc& desc)
	{
		TF_PROFILE_FUNCTION();
		TF_CORE_ASSERT(desc.ColorCount > 0 && desc.ColorCount <= 8, "RenderTarget needs 1 to 8 color attachments");

		Ref<RenderTarget> target(new RenderTarget());
		target->m_Impl = new Impl();
		target->m_Impl->Desc = desc;

		if (desc.Width == 0 || desc.Height == 0)
		{
			TF_CORE_ERROR("RenderTarget '{0}' needs a non-zero extent", desc.DebugName ? desc.DebugName : "<unnamed>");
			return target;
		}

		if (!target->m_Impl->BuildAttachments())
			target->m_Impl->DropAttachments();

		return target;
	}

	RenderTarget::~RenderTarget()
	{
		delete m_Impl;
		m_Impl = nullptr;
	}

	bool RenderTarget::Resize(uint32_t width, uint32_t height)
	{
		TF_PROFILE_FUNCTION();

		if (!m_Impl || width == 0 || height == 0)
			return false;

		if (width == m_Impl->Desc.Width && height == m_Impl->Desc.Height && IsValid())
			return false;

		if (m_Impl->Desc.Dim != Dimension::Tex2D)
		{
			TF_CORE_ERROR("RenderTarget '{0}' is not 2D; layered and cube tarngets are fixed-size",
				m_Impl->Desc.DebugName ? m_Impl->Desc.DebugName : "<unnamed>");
			return false;
		}

		m_Impl->Desc.Width = width;
		m_Impl->Desc.Height = height;

		m_Impl->DropAttachments();

		if (!m_Impl->BuildAttachments())
		{
			m_Impl->DropAttachments();
			return false;
		}

		return true;
	}

	uint32_t RenderTarget::GetWidth() const
	{
		return m_Impl ? m_Impl->Desc.Width : 0;
	}

	uint32_t RenderTarget::GetHeight() const
	{
		return m_Impl ? m_Impl->Desc.Height : 0;
	}

	uint32_t RenderTarget::GetColorCount() const
	{
		return m_Impl ? m_Impl->Desc.ColorCount : 0;
	}

	const Ref<Texture>& RenderTarget::GetColor(uint32_t index) const
	{
		if (!m_Impl || index >= m_Impl->Desc.ColorCount)
			return s_NullTexture;

		return m_Impl->Color[index];
	}

	const Ref<Texture>& RenderTarget::GetDepth() const
	{
		return m_Impl ? m_Impl->Depth : s_NullTexture;
	}

	Format RenderTarget::GetColorFormat(uint32_t index) const
	{
		if (!m_Impl || index >= m_Impl->Desc.ColorCount)
			return Format::Undefined;

		return m_Impl->Desc.ColorFormat[index];
	}

	Format RenderTarget::GetDepthFormat() const
	{
		return m_Impl ? m_Impl->Desc.DepthFormat : Format::Undefined;
	}

	bool RenderTarget::IsValid() const
	{
		if (!m_Impl || m_Impl->Desc.ColorCount == 0)
			return false;

		for (uint32_t i = 0; i < m_Impl->Desc.ColorCount; i++)
			if (!m_Impl->Color[i] || !m_Impl->Color[i]->IsValid())
				return false;

		return m_Impl->Desc.DepthFormat == Format::Undefined || (m_Impl->Depth && m_Impl->Depth->IsValid());
	}
}
