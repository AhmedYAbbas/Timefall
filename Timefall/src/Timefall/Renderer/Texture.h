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

		virtual void Bind(int slot = 0) const = 0;
	};

	class Texture2D : public Texture
	{
	public:
		static Ref<Texture> Create(const std::string& path);
	};
}