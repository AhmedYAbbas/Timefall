#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Timefall
{
	enum class ShaderStage : uint8_t
	{
		Vertex = 0,
		Fragment = 1
	};

	inline constexpr uint8_t StageBit(ShaderStage stage)
	{
		return (uint8_t)(1u << (uint8_t)stage);
	}

	enum class ShaderBindingType : uint8_t
	{
		UniformBuffer,
		StorageBuffer,
		SampledImage,
		Sampler
	};

	struct ShaderBinding
	{
		std::string Name;
		uint32_t Set = 0;
		uint32_t Binding = 0;
		uint32_t Count = 1; // 0 == runtime array (bindless)
		ShaderBindingType Type = ShaderBindingType::UniformBuffer;
	};

	struct ShaderInput
	{
		std::string Name;
		uint32_t Location = 0;
	};

	struct ShaderReflection
	{
		std::vector<ShaderBinding> Bindings;
		std::vector<ShaderInput> VertexInputs;
		uint32_t PushConstantSize = 0;

		uint8_t StageMask = 0;
		uint64_t Digest = 0;

		bool HasStage(ShaderStage stage) const { return (StageMask & StageBit(stage)) != 0; }
	};

	uint64_t ComputeReflectionDigest(const ShaderReflection& reflection);
}
