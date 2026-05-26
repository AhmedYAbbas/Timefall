#pragma once

#include "Timefall/Core/Core.h"
#include "Timefall/Renderer/Texture.h"

#include <filesystem>

namespace Timefall
{
	struct MSDFData;

	class TF_API Font
	{
	public:
		Font(const std::filesystem::path& filepath);
		~Font();

		const MSDFData* GetMSDFData() const { return m_MSDFData; }
		Ref<Texture2D> GetAtlasTexture() const { return m_AtlasTexture; }

		static Ref<Font> GetDefault();

	private:
		MSDFData* m_MSDFData;
		Ref<Texture2D> m_AtlasTexture;
	};
}