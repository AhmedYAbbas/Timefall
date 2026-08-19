#pragma once

#include "Timefall/Core/Core.h"

#include <imgui.h>

namespace Timefall
{
	class Texture2D;

	namespace RHI
	{
		class Texture;
	}

	namespace UI
	{
		TF_API ImTextureID GetTextureID(const Ref<RHI::Texture>& texture);
		TF_API ImTextureID GetTextureID(const Ref<Texture2D>& texture);
		TF_API void SetBackendAlive(bool alive);
	}
}
