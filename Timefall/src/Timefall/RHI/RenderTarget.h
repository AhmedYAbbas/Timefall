#pragma once

#include "Timefall/Core/Core.h"
#include "Timefall/RHI/RHITypes.h"
#include "Timefall/RHI/Texture.h"

namespace Timefall::RHI
{
	struct RenderTargetDesc
	{
		uint32_t Width = 1;
		uint32_t Height = 1;
		Format ColorFormat[8]{};
		uint32_t ColorCount = 1;
		TextureUsage ColorUsage[8]{}; // extra per-attachment usage, OR'd onto Sampled|ColorAttachment
		Format DepthFormat = Format::Undefined; // Undefined == no depth attachment
		const char* DebugName = nullptr;
	};

	class TF_API RenderTarget
	{
	public:
		static Ref<RenderTarget> Create(const RenderTargetDesc& desc);

		~RenderTarget();

		RenderTarget(const RenderTarget&) = delete;
		RenderTarget& operator=(const RenderTarget&) = delete;

		bool Resize(uint32_t width, uint32_t height);

		uint32_t GetWidth() const;
		uint32_t GetHeight() const;
		uint32_t GetColorCount() const;

		const Ref<Texture>& GetColor(uint32_t index) const;
		const Ref<Texture>& GetDepth() const;

		Format GetColorFormat(uint32_t index) const;
		Format GetDepthFormat() const;

		bool IsValid() const;

	private:
		RenderTarget() = default;

		struct Impl;
		Impl* m_Impl = nullptr;
	};
}
