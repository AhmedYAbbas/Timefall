#pragma once

#include "Timefall/RHI/Bindings.h"

#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>

namespace Timefall
{
	inline constexpr uint32_t MAX_DIR_LIGHTS = 4;
	inline constexpr uint32_t MAX_POINT_LIGHTS = 32;
	inline constexpr uint32_t MAX_SPOT_LIGHTS = 16;
	inline constexpr uint32_t MAX_CASCADES = 4;

	struct GpuDirLight
	{
		glm::vec4 Direction;
		glm::vec4 Color; // Color.a = intensity
	};

	struct GpuPointLight
	{
		glm::vec4 Position; // Position.w = range
		glm::vec4 Color; // Color.a = intensity
	};

	struct GpuSpotLight
	{
		glm::vec4 Position;
		glm::vec4 Direction;
		glm::vec4 Color;
		glm::vec4 Params; // (range, innerCos, outerCos, intensity)
	};

	// Set 1 binding 0. The twin of PassUniforms in Common/Bindings.slang - fields are appended, never reordered
	struct PassUniforms
	{
		glm::mat4 ViewProjection{1.0f};
		glm::mat4 View{1.0f};
		glm::vec4 CameraPosition{0.0f};

		GpuDirLight DirLights[MAX_DIR_LIGHTS]{};
		GpuPointLight PointLights[MAX_POINT_LIGHTS]{};
		GpuSpotLight SpotLights[MAX_SPOT_LIGHTS]{};
		uint32_t DirCount = 0;
		uint32_t PointCount = 0;
		uint32_t SpotCount = 0;
		uint32_t _LightPad = 0;

		float EnvIntensity = 1.0f;
		float EnvRotation = 0.0f;
		float MaxReflectionLod = 4.0f;
		int32_t HasEnvironment = 0;
		uint32_t IrradianceIndex = 0;
		uint32_t PrefilterIndex = 0;
		uint32_t SkyboxIndex = 0;
		uint32_t _EnvPad = 0;

		glm::mat4 LightViewProj[MAX_CASCADES]{};
		glm::vec4 CascadeSplits{0.0f};
		glm::vec4 CascadeTexelWorld{0.0f};
		glm::vec4 CascadeDepthRange{0.0f};
		uint32_t CascadeCount = 0;
		uint32_t VisualizeCascades = 0;
		float LightSize = 0.08f;
		float DepthBias = 1.0f;
		float CascadeBlend = 0.1f;
		int32_t BlockerSamples = 16;
		int32_t PCFSamples = 16;
		int32_t SoftShadows = 1;

		glm::mat4 SpotLightViewProj[MAX_SPOT_LIGHTS]{};
		glm::vec4 SpotShadowParams[MAX_SPOT_LIGHTS]{}; // x = casts, y = lightSize, z = depthBias, w = atlasLayer

		glm::vec4 PointShadowParams[MAX_POINT_LIGHTS]{}; // x = casts, y = lightSize, z = depthBias, w = cubeLayer

		uint32_t SunShadowIndex = 0; // g_TextureArrays slot of the cascade depth array; 0 = white fallback
		uint32_t SpotShadowIndex = 0; // g_TextureArrays slot of the spot atlas; 0 = white fallback
		uint32_t PointShadowIndex = 0; // g_TextureCubeArrays slot of the point cubes; 0 = white fallback

		float SunShadowTexel = 1.0f; // 1 / ShadowMapResolution, in UV
		float SpotShadowTexel = 1.0f; // 1 / SpotShadowResolution, in UV
		float PointShadowTexel = 1.0f; // 1 / PointShadowResolution, in UV
		uint32_t _ShadowPad2 = 0;
		uint32_t _ShadowPad3 = 0;
	};

	// Measured against slangc: these are the offsets Slang gives the std140 twin.
	static_assert(sizeof(PassUniforms) == 4528);
	static_assert(offsetof(PassUniforms, DirLights) == 144);
	static_assert(offsetof(PassUniforms, PointLights) == 272);
	static_assert(offsetof(PassUniforms, SpotLights) == 1296);
	static_assert(offsetof(PassUniforms, DirCount) == 2320);
	static_assert(offsetof(PassUniforms, EnvIntensity) == 2336);
	static_assert(offsetof(PassUniforms, LightViewProj) == 2368);
	static_assert(offsetof(PassUniforms, SpotLightViewProj) == 2704);
	static_assert(offsetof(PassUniforms, SpotShadowParams) == 3728);
	static_assert(offsetof(PassUniforms, PointShadowParams) == 3984);
	static_assert(offsetof(PassUniforms, SunShadowIndex) == 4496);
	static_assert(offsetof(PassUniforms, SpotShadowTexel) == 4512);
	static_assert(offsetof(PassUniforms, PointShadowIndex) == 4504);
	static_assert(offsetof(PassUniforms, PointShadowTexel) == 4516);

	// The slice must hold the struct; the descriptor's range is PassUniformSlotBytes
	static_assert(sizeof(PassUniforms) <= RHI::PassUniformSlotBytes);
}
