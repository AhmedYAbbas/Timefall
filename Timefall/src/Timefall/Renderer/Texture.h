#pragma once

#include "Timefall/Core/Core.h"
#include "Timefall/Core/Buffer.h"
#include "Timefall/Asset/Asset.h"

namespace Timefall
{
	enum class TF_API ImageFormat { None = 0, R8, RGB8, RGBA8, RGB32F };

	struct TF_API TextureSpecification
	{
		uint32_t Width = 1;
		uint32_t Height = 1;
		ImageFormat Format = ImageFormat::RGBA8;
		bool GenerateMips = true;
	};

	class TF_API Texture : public Asset
	{
	public:
		virtual ~Texture() = default;

		virtual const TextureSpecification& GetSpecification() const = 0;

		virtual uint32_t GetWidth() const = 0;
		virtual uint32_t GetHeight() const = 0;
		virtual uint32_t GetRendererID() const = 0;

		// OpenGL specific?
		virtual uint32_t GetInternalFormat() const = 0;
		virtual uint32_t GetDataFormat() const = 0;

		virtual std::vector<uint8_t> GetData() const = 0;
		virtual void SetData(Buffer data) = 0;
		virtual void SetData(const std::vector<uint8_t>& data, uint32_t dataFormat) = 0;

		virtual void Bind(int slot = 0) const = 0;
		// Bind an sRGB-decoding view of this texture's storage (for albedo/color maps).
		virtual void BindAsSRGB(int slot = 0) const = 0;

		virtual bool operator==(const Texture& other) const = 0;
	};

	class TF_API Texture2D : public Texture
	{
	public:
		static Ref<Texture2D> Create(const TextureSpecification& spec, Buffer data = Buffer());

		static AssetType GetStaticType() { return AssetType::Texture2D; }
		virtual AssetType GetType() const override { return GetStaticType(); }
	};
}