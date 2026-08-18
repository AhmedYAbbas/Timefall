#include "tfpch.h"

#include "Timefall/Renderer/Texture.h"

namespace Timefall
{
	static RHI::Format ToRHIFormat(ImageFormat format)
	{
		switch (format)
		{
			case ImageFormat::R8: return RHI::Format::R8Unorm;
			case ImageFormat::RGB8: // widened
			case ImageFormat::RGBA8: return RHI::Format::RGBA8Unorm;
			case ImageFormat::RGB32F: return RHI::Format::RGBA32F;
			default: return RHI::Format::Undefined;
		}
	}

	static bool NeedsExpansion(ImageFormat format)
	{
		return format == ImageFormat::RGB8 || format == ImageFormat::RGB32F;
	}

	static std::vector<uint8_t> ExpandChannels(const uint8_t* source, uint64_t pixels, ImageFormat format)
	{
		if (format == ImageFormat::RGB32F)
		{
			std::vector<uint8_t> out(pixels * 4 * sizeof(float));
			auto* dst = (float*)out.data();
			auto* src = (const float*)source;

			for (uint64_t i = 0; i < pixels; i++)
			{
				dst[i * 4 + 0] = src[i * 3 + 0];
				dst[i * 4 + 1] = src[i * 3 + 1];
				dst[i * 4 + 2] = src[i * 3 + 2];
				dst[i * 4 + 3] = 1.0f;
			}

			return out;
		}

		std::vector<uint8_t> out(pixels * 4);
		for (uint64_t i = 0; i < pixels; i++)
		{
			out[i * 4 + 0] = source[i * 3 + 0];
			out[i * 4 + 1] = source[i * 3 + 1];
			out[i * 4 + 2] = source[i * 3 + 2];
			out[i * 4 + 3] = 255;
		}

		return out;
	}

	Texture2D::Texture2D(const TextureSpecification& spec, Buffer data)
		: m_Spec(spec)
	{
		const RHI::Format format = ToRHIFormat(spec.Format);
		if (format == RHI::Format::Undefined)
		{
			TF_CORE_ERROR("Texture2D created with an unsupported ImageFormat ({0})", (int)spec.Format);
			return;
		}

		const RHI::TextureDesc desc{.Width = spec.Width,
			.Height = spec.Height,
			.PixelFormat = format,
			.MipLevels = spec.GenerateMips ? 0u : 1u,
			.SRGBView = format == RHI::Format::RGBA8Unorm,
			.DebugName = m_Spec.DebugName.empty() ? nullptr : m_Spec.DebugName.c_str()};
		m_Texture = RHI::Texture::Create(desc);

		if (data)
			SetData(data);
	}

	Ref<Texture2D> Texture2D::Create(const TextureSpecification& spec, Buffer data)
	{
		return Ref<Texture2D>(new Texture2D(spec, data));
	}

	void Texture2D::SetData(Buffer data)
	{
		if (!m_Texture || !m_Texture->IsValid() || !data)
			return;

		const uint64_t pixels = (uint64_t)m_Spec.Width * m_Spec.Height;

		if (NeedsExpansion(m_Spec.Format))
		{
			const std::vector<uint8_t> expanded = ExpandChannels((const uint8_t*)data.Data, pixels, m_Spec.Format);
			m_Texture->Upload(expanded.data(), expanded.size());
			return;
		}

		m_Texture->Upload(data.Data, data.Size);
	}

	bool Texture2D::IsValid() const
	{
		return m_Texture && m_Texture->IsValid();
	}
}
