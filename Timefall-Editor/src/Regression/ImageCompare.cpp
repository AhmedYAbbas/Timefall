#include "ImageCompare.h"

#include "Timefall/Core/Log.h"

#include <stb_image/stb_image.h>
#include <stb_image/stb_image_write.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace Timefall
{
	void WritePng(const std::filesystem::path& path, std::span<const std::byte> rgba, uint32_t width, uint32_t height)
	{
		stbi_write_png(path.string().c_str(), (int)width, (int)height, 4, rgba.data(), (int)width * 4);
	}

	ImageCompareResult CompareImageToGolden(std::span<const std::byte> actual, uint32_t width, uint32_t height,
		const std::filesystem::path& goldenPath, int pixelThreshold, double maxDifferingFraction)
	{
		ImageCompareResult result;

		int gw, gh, gc;
		stbi_uc* golden = stbi_load(goldenPath.string().c_str(), &gw, &gh, &gc, 4);
		if (!golden)
		{
			TF_CORE_ERROR("Regression: cannot load golden image '{0}'", goldenPath.string());
			return result;
		}

		if ((uint32_t)gw != width || (uint32_t)gh != height)
		{
			TF_CORE_ERROR("Regression: golden '{0}' is {1}x{2}, case rendered {3}x{4}", goldenPath.string(), gw, gh, width, height);
			stbi_image_free(golden);
			return result;
		}

		const size_t pixelCount = (size_t)width * height;
		size_t differing = 0;
		double squaredError = 0.0;

		const auto* actualBytes = reinterpret_cast<const uint8_t*>(actual.data());
		for (size_t i = 0; i < pixelCount; i++)
		{
			int maxDiff = 0;
			for (int c = 0; c < 4; c++)
			{
				int diff = std::abs((int)actualBytes[i * 4 + c] - (int)golden[i * 4 + c]);
				maxDiff = std::max(maxDiff, diff);
				squaredError += (double)diff * diff;
			}

			if (maxDiff > pixelThreshold)
				differing++;

			result.MaxDifference = std::max(result.MaxDifference, maxDiff);
		}

		stbi_image_free(golden);

		result.DifferingFraction = (double)differing / (double)pixelCount;
		const double mse = squaredError / (double)(pixelCount * 4);
		result.PSNR = mse > 0.0 ? 10.0 * std::log10((255.0 * 255.0) / mse) : 100.0;
		result.Passed = result.DifferingFraction <= maxDifferingFraction;

		return result;
	}

	void WriteFailureImages(std::span<const std::byte> actual, uint32_t width, uint32_t height, const std::filesystem::path& goldenPath,
		const std::filesystem::path& outputDir)
	{
		int gw, gh, gc;
		stbi_uc* golden = stbi_load(goldenPath.string().c_str(), &gw, &gh, &gc, 4);
		if (!golden || (uint32_t)gw != width || (uint32_t)gh != height)
		{
			if (golden)
				stbi_image_free(golden);
			return;
		}

		const auto* actualBytes = reinterpret_cast<const uint8_t*>(actual.data());
		std::vector<uint8_t> diff((size_t)width * height * 4);

		for (size_t i = 0; i < (size_t)width * height; i++)
		{
			int maxDiff = 0;
			for (int c = 0; c < 4; c++)
				maxDiff = std::max(maxDiff, std::abs((int)actualBytes[i * 4 + c] - (int)golden[i * 4 + c]));

			const uint8_t gray = (uint8_t)(((int)golden[i * 4 + 0] + golden[i * 4 + 1] + golden[i * 4 + 2]) / 3);
			if (maxDiff > 0)
			{
				diff[i * 4 + 0] = (uint8_t)std::min(255, maxDiff * 10);
				diff[i * 4 + 1] = 0;
				diff[i * 4 + 2] = 0;
			}
			else
			{
				diff[i * 4 + 0] = diff[i * 4 + 1] = diff[i * 4 + 2] = gray;
			}
			diff[i * 4 + 3] = 255;
		}

		stbi_image_free(golden);
		stbi_write_png((outputDir / "diff.png").string().c_str(), (int)width, (int)height, 4, diff.data(), (int)width * 4);
	}
}
