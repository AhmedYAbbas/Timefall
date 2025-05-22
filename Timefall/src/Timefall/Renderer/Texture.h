#pragma once

#include "Timefall/Core/Core.h"

namespace Timefall
{
	class Texture
	{
	public:
		virtual ~Texture() = default;

		virtual uint32_t GetWidth() const = 0;
		virtual uint32_t GetHeight() const = 0;
		virtual uint32_t GetRendererID() const = 0;
		virtual std::filesystem::path GetPath() const = 0;
		
		// OpenGL specific?
		virtual uint32_t GetInternalFormat() const = 0;
		virtual uint32_t GetDataFormat() const = 0;

		virtual std::vector<uint8_t> GetData() const = 0;
		virtual void SetData(void* data, uint32_t size) = 0;
		virtual void SetData(const std::vector<uint8_t>& data, uint32_t dataFormat) = 0;

		virtual void Bind(int slot = 0) const = 0;

		virtual bool operator==(const Texture& other) const = 0;
	};

	class Texture2D : public Texture
	{
	public:
		static Ref<Texture2D> Create(uint32_t width, uint32_t height);
		static Ref<Texture2D> Create(const std::string& path);
	};
}