#include "tfpch.h"
#include "TextureImporter.h"

#include "Timefall/Project/Project.h"

#include <stb_image.h>

namespace Timefall
{
	Ref<Texture2D> TextureImporter::ImportTexture2D(AssetHandle handle, const AssetMetadata& metadata)
	{
		TF_PROFILE_FUNCTION();

		return LoadTexture2D(Project::GetAssetDirectory() / metadata.FilePath);
	}

	Ref<Texture2D> TextureImporter::LoadTexture2D(const std::filesystem::path& path)
	{
		TF_PROFILE_FUNCTION();

		int width, height, channels;
		Buffer data;
		stbi_set_flip_vertically_on_load(1);

		bool isHDR = stbi_is_hdr(path.string().c_str());

		if (isHDR)
		{
			// Equirectangular HDR: keep full float precision, RGB, no mips (only the
			// equirect->cube pass ever samples it).
			float* pixels;
			{
				TF_PROFILE_SCOPE("stbi_loadf() - TextureImporter::LoadTexture2D (HDR)");
				pixels = stbi_loadf(path.string().c_str(), &width, &height, &channels, 3);
			}
			if (!pixels)
			{
				TF_CORE_ERROR("TextureImporter - Failed to load HDR: {0}", path.string());
				return nullptr;
			}

			TextureSpecification spec;
			spec.Width = width;
			spec.Height = height;
			spec.Format = ImageFormat::RGB32F;
			spec.GenerateMips = false;

			data.Data = (uint8_t*)pixels;
			data.Size = (uint64_t)width * height * 3 * sizeof(float);
			Ref<Texture2D> texture = Texture2D::Create(spec, data);
			stbi_image_free(pixels);
			return texture;
		}

		// Grayscale (1) and gray+alpha (2) maps expand to RGB so the shader's per-channel reads
		// (.r AO, .g roughness, .b metallic) all see the gray value; packed RGB/RGBA maps load as-is.
		int reqChannels = 0;
		{
			int probeW, probeH, probeChannels;
			if (stbi_info(path.string().c_str(), &probeW, &probeH, &probeChannels) && probeChannels < 3)
				reqChannels = 3;
		}

		{
			TF_PROFILE_SCOPE("stbi_load() - TextureImporter::ImportTexture2D");
			data.Data = stbi_load(path.string().c_str(), &width, &height, &channels, reqChannels);
		}

		if (!data.Data)
		{
			TF_CORE_ERROR("TextureImporter::ImportTexture2D - Failed to load texture: {0}", path.string());
			return nullptr;
		}

		// stbi reports the file's native channel count even when it converts to reqChannels.
		int loadedChannels = reqChannels != 0 ? reqChannels : channels;
		data.Size = width * height * loadedChannels;

		TextureSpecification spec;
		spec.Width = width;
		spec.Height = height;

		switch (loadedChannels)
		{
		case 3:
			spec.Format = ImageFormat::RGB8;
			break;
		case 4:
			spec.Format = ImageFormat::RGBA8;
			break;
		default:
			TF_CORE_ERROR("TextureImporter - Unsupported channel count {0} for {1}", loadedChannels, path.string());
			data.Release();
			return nullptr;
		}

		Ref<Texture2D> texture = Texture2D::Create(spec, data);
		data.Release();
		return texture;
	}
}