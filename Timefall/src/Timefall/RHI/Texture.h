#pragma once

#include "Timefall/Core/Core.h"
#include "Timefall/RHI/RHITypes.h"

#include <cstdint>

namespace Timefall::RHI
{
	struct TextureDesc
	{
		uint32_t Width = 1;
		uint32_t Height = 1;
		Format PixelFormat = Format::RGBA8Unorm;
		uint32_t MipLevels = 1; // 0 == full chain derived from Width/Height
		Dimension Dim = Dimension::Tex2D;
		uint32_t ArrayLayers = 1;
		bool SRGBView = false;
		TextureUsage Usage = TextureUsage::Sampled;
		const char* DebugName = nullptr;
	};

	class TF_API Texture
	{
	public:
		static Ref<Texture> Create(const TextureDesc& desc);
		static Ref<Texture> CreateWithData(const TextureDesc& desc, const void* data, uint64_t size);

		~Texture();

		Texture(const Texture&) = delete;
		Texture& operator=(const Texture&) = delete;

		bool Upload(const void* data, uint64_t size);

		uint32_t GetWidth() const;
		uint32_t GetHeight() const;
		uint32_t GetMipLevels() const;
		Format GetPixelFormat() const;
		bool IsValid() const;

		void* GetNativeView(bool srgb = false) const;
		uint32_t GetBindlessIndex(bool srgb = false);

		void* GetUIHandle() const;
		void SetUIHandle(void* handle, void (*destroy)(void*));

	private:
		Texture() = default;

		friend class CommandList;

		struct Impl;
		Impl* m_Impl = nullptr;
	};
}
