#pragma once

#include "Timefall/Core/Core.h"
#include "Timefall/Core/Buffer.h"
#include "Timefall/Asset/Asset.h"
#include "Timefall/RHI/Texture.h"

namespace Timefall
{
	enum class TF_API ImageFormat { None = 0, R8, RGB8, RGBA8, RGB32F };

	struct TF_API TextureSpecification
	{
		uint32_t Width = 1;
		uint32_t Height = 1;
		ImageFormat Format = ImageFormat::RGBA8;
		bool GenerateMips = true;
		std::string DebugName; // owned: the RHI desc takes a raw pointer and copies at name-set time
	};

	class TF_API Texture2D final : public Asset
	{
	public:
		static Ref<Texture2D> Create(const TextureSpecification& spec, Buffer data = Buffer());

		const TextureSpecification& GetSpecification() const { return m_Spec; }

		uint32_t GetWidth() const { return m_Spec.Width; }
		uint32_t GetHeight() const { return m_Spec.Height; }

		void SetData(Buffer data);

		const Ref<RHI::Texture>& GetRHITexture() const { return m_Texture; }
		bool IsValid() const;

		virtual bool operator==(const Texture2D& other) const { return this == &other; }

		static AssetType GetStaticType() { return AssetType::Texture2D; }
		virtual AssetType GetType() const override { return GetStaticType(); }

	private:
		Texture2D(const TextureSpecification& spec, Buffer data);

		TextureSpecification m_Spec;
		Ref<RHI::Texture> m_Texture;
	};
}
