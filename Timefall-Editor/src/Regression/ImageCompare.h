#pragma once

#include <cstdint>
#include <cstddef>
#include <filesystem>
#include <span>

namespace Timefall
{
	struct ImageCompareResult
	{
		bool Passed = false;
		double DifferingFraction = 0.0;
		int MaxDifference = 0;
		double PSNR = 0.0;
	};

	void WritePng(const std::filesystem::path& path, std::span<const std::byte> rgba, uint32_t width, uint32_t height);

	// A pixel differs when its largest channel difference exceeds pixelThreshold
	ImageCompareResult CompareImageToGolden(std::span<const std::byte> actual, uint32_t width, uint32_t height,
		const std::filesystem::path& goldenPath, int pixelThreshold, double maxDifferingFraction);

	// Writes diff.png into outputDir: a greyscale copy of the golden with differences painted red, amplified x10
	void WriteFailureImages(std::span<const std::byte> actual, uint32_t width, uint32_t height, const std::filesystem::path& goldenPath,
		const std::filesystem::path& outputDir);
}
