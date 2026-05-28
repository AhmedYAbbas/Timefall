#pragma once

#include "Timefall/Core/Core.h"
#include "Timefall/Asset/AssetMetadata.h"

#include "Timefall/Renderer/Texture.h"

namespace Timefall
{
	class TF_API TextureImporter
	{
	public:
		// AssetMetadata filepath is relative to project's asset directory, not filesystem
		static Ref<Texture2D> ImportTexture2D(AssetHandle handle, const AssetMetadata& metadata);

		// Reads file directory from filesystem
		// (i.e. path has to be relative/absolute to working directory)
		static Ref<Texture2D> LoadTexture2D(const std::filesystem::path& path);
	};
}