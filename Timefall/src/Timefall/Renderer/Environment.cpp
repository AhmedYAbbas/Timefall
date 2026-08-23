#include "tfpch.h"
#include "Timefall/Renderer/Environment.h"

#include "Timefall/Renderer/Mesh.h"
#include "Timefall/Renderer/ShaderLibrary.h"
#include "Timefall/RHI/CommandList.h"
#include "Timefall/RHI/GpuBuffer.h"
#include "Timefall/RHI/Pipeline.h"
#include "Timefall/RHI/RenderTarget.h"
#include "Timefall/RHI/RenderDevice.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>

namespace Timefall
{
	static constexpr uint32_t kSkyboxSize = 512;
	static constexpr uint32_t kIrradianceSize = 32;
	static constexpr uint32_t kPrefilterSize = 128;
	static constexpr uint32_t kPrefilterMips = 5; // MaxReflectionLod = 4, matching PassUniforms' default
	static constexpr RHI::Format kCubeFormat = RHI::Format::RGBA16F;

	// Cube faces are stored top-down, so the RHI's Y-flipped viewport has to be cancelled for the bake
	static const glm::mat4 s_CaptureProj = [] {
		glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
		proj[1] *= -1.0f;
		return proj;
	}();
	static const glm::mat4 s_CaptureViews[6] = {
		glm::lookAt(glm::vec3(0.0f), glm::vec3(1, 0, 0), glm::vec3(0, -1, 0)),
		glm::lookAt(glm::vec3(0.0f), glm::vec3(-1, 0, 0), glm::vec3(0, -1, 0)),
		glm::lookAt(glm::vec3(0.0f), glm::vec3(0, 1, 0), glm::vec3(0, 0, 1)),
		glm::lookAt(glm::vec3(0.0f), glm::vec3(0, -1, 0), glm::vec3(0, 0, -1)),
		glm::lookAt(glm::vec3(0.0f), glm::vec3(0, 0, 1), glm::vec3(0, -1, 0)),
		glm::lookAt(glm::vec3(0.0f), glm::vec3(0, 0, -1), glm::vec3(0, -1, 0))
	};

	static const char* s_FacePassNames[6] = {
		"EquirectToCube +X", "EquirectToCube -X", "EquirectToCube +Y", "EquirectToCube -Y", "EquirectToCube +Z", "EquirectToCube -Z"};
	static const char* s_IrradiancePassNames[6] = {"IrradianceConvolve +X", "IrradianceConvolve -X", "IrradianceConvolve +Y",
		"IrradianceConvolve -Y", "IrradianceConvolve +Z", "IrradianceConvolve -Z"};
	static const char* s_PrefilterPassNames[5] = {"Prefilter mip0", "Prefilter mip1", "Prefilter mip2", "Prefilter mip3", "Prefilter mip4"};

	struct EquirectPush
	{
		glm::mat4 ViewProjection{1.0f};
		uint32_t EquirectIndex = 0;
		uint32_t _Pad[3]{};
	};
	static_assert(sizeof(EquirectPush) == 80);

	struct IrradiancePush
	{
		glm::mat4 ViewProjection{1.0f};
		uint32_t SkyboxIndex = 0;
		uint32_t _Pad[3]{};
	};
	static_assert(sizeof(IrradiancePush) == 80);

	struct PrefilterPush
	{
		glm::mat4 ViewProjection{1.0f};
		uint32_t SkyboxIndex = 0;
		float Roughness = 0.0f;
		float EnvResolution = 0.0f;
		uint32_t _Pad = 0;
	};
	static_assert(sizeof(PrefilterPush) == 80);

	static Ref<MeshSource> s_CubeMesh;
	static Ref<Shader> s_EquirectShader;
	static Ref<RHI::GraphicsPipeline> s_EquirectPipeline;

	static Ref<Shader> s_IrradianceShader;
	static Ref<RHI::GraphicsPipeline> s_IrradiancePipeline;

	static Ref<Shader> s_PrefilterShader;
	static Ref<RHI::GraphicsPipeline> s_PrefilterPipeline;

	static bool EnsureBakeResources()
	{
		if (!s_CubeMesh)
		{
			RHI::UploadScope upload;
			s_CubeMesh = MeshSource::CreateCube();
		}

		if (!s_EquirectShader)
			s_EquirectShader = ShaderLibrary::Load("Assets/Shaders/Renderer3D_EquirectToCube.slang");

		if (!s_EquirectPipeline)
		{
			RHI::GraphicsPipelineDesc desc;
			desc.ShaderModule = s_EquirectShader;
			desc.VertexLayout = {{ShaderDataType::Float3, "a_Position"}, {ShaderDataType::Float3, "a_Normal"},
				{ShaderDataType::Float2, "a_TexCoord"}, {ShaderDataType::Float3, "a_Tangent"}, {ShaderDataType::Float3, "a_Bitangent"}};
			desc.Primitive = RHI::Topology::TriangleList;
			desc.ColorFormats[0] = kCubeFormat;
			desc.ColorCount = 1;
			desc.DepthFormat = RHI::Format::Undefined;
			desc.Depth = {.Test = false, .Write = false};
			desc.Blend = RHI::BlendMode::None;
			desc.Raster.Cull = RHI::CullMode::None;
			desc.DebugName = "EquirectToCubePipeline";

			s_EquirectPipeline = RHI::GraphicsPipeline::Create(desc);
		}

		if (!s_IrradianceShader)
			s_IrradianceShader = ShaderLibrary::Load("Assets/Shaders/Renderer3D_IrradianceConvolve.slang");

		if (!s_IrradiancePipeline)
		{
			RHI::GraphicsPipelineDesc desc;
			desc.ShaderModule = s_IrradianceShader;
			desc.VertexLayout = {{ShaderDataType::Float3, "a_Position"}, {ShaderDataType::Float3, "a_Normal"},
				{ShaderDataType::Float2, "a_TexCoord"}, {ShaderDataType::Float3, "a_Tangent"}, {ShaderDataType::Float3, "a_Bitangent"}};
			desc.Primitive = RHI::Topology::TriangleList;
			desc.ColorFormats[0] = kCubeFormat;
			desc.ColorCount = 1;
			desc.DepthFormat = RHI::Format::Undefined;
			desc.Depth = {.Test = false, .Write = false};
			desc.Blend = RHI::BlendMode::None;
			desc.Raster.Cull = RHI::CullMode::None;
			desc.DebugName = "IrradianceConvolvePipeline";

			s_IrradiancePipeline = RHI::GraphicsPipeline::Create(desc);
		}

		if (!s_PrefilterShader)
			s_PrefilterShader = ShaderLibrary::Load("Assets/Shaders/Renderer3D_Prefilter.slang");

		if (!s_PrefilterPipeline)
		{
			RHI::GraphicsPipelineDesc desc;
			desc.ShaderModule = s_PrefilterShader;
			desc.VertexLayout = {{ShaderDataType::Float3, "a_Position"}, {ShaderDataType::Float3, "a_Normal"},
				{ShaderDataType::Float2, "a_TexCoord"}, {ShaderDataType::Float3, "a_Tangent"}, {ShaderDataType::Float3, "a_Bitangent"}};
			desc.Primitive = RHI::Topology::TriangleList;
			desc.ColorFormats[0] = kCubeFormat;
			desc.ColorCount = 1;
			desc.DepthFormat = RHI::Format::Undefined;
			desc.Depth = {.Test = false, .Write = false};
			desc.Blend = RHI::BlendMode::None;
			desc.Raster.Cull = RHI::CullMode::None;
			desc.DebugName = "PrefilterPipeline";

			s_PrefilterPipeline = RHI::GraphicsPipeline::Create(desc);
		}

		return s_CubeMesh && s_CubeMesh->HasGpuBuffers() && s_EquirectPipeline && s_EquirectPipeline->IsValid() && s_IrradiancePipeline && s_IrradiancePipeline->IsValid() && s_PrefilterPipeline && s_PrefilterPipeline->IsValid();
	}

	Ref<Environment> Environment::Create(const Ref<Texture2D>& equirect)
	{
		TF_PROFILE_FUNCTION();

		Ref<Environment> environment(new Environment());

		if (!equirect || !equirect->GetRHITexture() || !equirect->GetRHITexture()->IsValid())
		{
			TF_CORE_ERROR("Environment::Create got no usable equirectangular texture");
			return environment;
		}

		if (!EnsureBakeResources())
		{
			TF_CORE_ERROR("Environment::Create could not build the bake mesh, shader or pipeline");
			return environment;
		}

		const uint32_t mips = 1 + (uint32_t)std::floor(std::log2((float)kSkyboxSize));

		environment->m_Skybox = RHI::RenderTarget::Create({.Width = kSkyboxSize,
			.Height = kSkyboxSize,
			.ColorFormat = {kCubeFormat},
			.ColorCount = 1,
			.ColorUsage = {RHI::TextureUsage::TransferSrc | RHI::TextureUsage::TransferDst},
			.DepthFormat = RHI::Format::Undefined,
			.Dim = RHI::Dimension::Cube,
			.ArrayLayers = 6,
			.MipLevels = mips,
			.DebugName = "EnvironmentSkybox"});

		if (!environment->m_Skybox || !environment->m_Skybox->IsValid())
		{
			TF_CORE_ERROR("Environment::Create could not allocate the {0}x{0} skybox cube", kSkyboxSize);
			environment->m_Skybox.reset();
			return environment;
		}

		environment->m_Irradiance = RHI::RenderTarget::Create({.Width = kIrradianceSize,
			.Height = kIrradianceSize,
			.ColorFormat = {kCubeFormat},
			.ColorCount = 1,
			.DepthFormat = RHI::Format::Undefined,
			.Dim = RHI::Dimension::Cube,
			.ArrayLayers = 6,
			.MipLevels = 1,
			.DebugName = "EnvironmentIrradiance"});

		if (!environment->m_Irradiance || !environment->m_Irradiance->IsValid())
		{
			TF_CORE_ERROR("Environment::Create could not allocate the {0}x{0} irradiance cube", kIrradianceSize);
			environment->m_Skybox.reset();
			environment->m_Irradiance.reset();
			return environment;
		}

		environment->m_Prefilter = RHI::RenderTarget::Create({.Width = kPrefilterSize,
			.Height = kPrefilterSize,
			.ColorFormat = {kCubeFormat},
			.ColorCount = 1,
			.DepthFormat = RHI::Format::Undefined,
			.Dim = RHI::Dimension::Cube,
			.ArrayLayers = 6,
			.MipLevels = kPrefilterMips,
			.DebugName = "EnvironmentPrefilter"});

		if (!environment->m_Prefilter || !environment->m_Prefilter->IsValid())
		{
			TF_CORE_ERROR("Environment::Create could not allocate the {0}x{0} prefilter cube", kPrefilterSize);
			environment->m_Skybox.reset();
			environment->m_Irradiance.reset();
			environment->m_Prefilter.reset();
			return environment;
		}

		const uint32_t equirectIndex = equirect->GetRHITexture()->GetBindlessIndex(false);
		const uint32_t skyboxIndex = environment->m_Skybox->GetColor(0)->GetBindlessIndex(false);
		const Submesh& submesh = s_CubeMesh->GetSubmeshes()[0];

		RHI::RenderDevice::Get().ExecuteImmediate([&](RHI::CommandList& cmd) {
			for (uint32_t face = 0; face < 6; face++)
			{
				cmd.BeginPass({.DebugName = s_FacePassNames[face],
					.Target = environment->m_Skybox.get(),
					.Layer = face,
					.Mip = 0,
					.Color = {{.Load = RHI::LoadOp::Clear, .ClearValue = {0.0f, 0.0f, 0.0f, 1.0f}}}});

				cmd.BindPipeline(*s_EquirectPipeline);
				cmd.BindVertexBuffer(*s_CubeMesh->GetVertexBuffer());
				cmd.BindIndexBuffer(*s_CubeMesh->GetIndexBuffer(), RHI::IndexType::U32);

				const EquirectPush push{.ViewProjection = s_CaptureProj * s_CaptureViews[face], .EquirectIndex = equirectIndex};
				cmd.PushConstants(&push, sizeof(push));

				cmd.DrawIndexed(submesh.IndexCount, 1, submesh.BaseIndex, (int32_t)submesh.BaseVertex);
				cmd.EndPass();
			}

			cmd.GenerateMips(*environment->m_Skybox->GetColor(0));

			for (uint32_t face = 0; face < 6; face++)
			{
				cmd.BeginPass({.DebugName = s_IrradiancePassNames[face],
					.Target = environment->m_Irradiance.get(),
					.Layer = face,
					.Mip = 0,
					.Color{{.Load = RHI::LoadOp::Clear, .ClearValue = {0.0f, 0.0f, 0.0f, 1.0f}}}});

				cmd.BindPipeline(*s_IrradiancePipeline);
				cmd.BindVertexBuffer(*s_CubeMesh->GetVertexBuffer());
				cmd.BindIndexBuffer(*s_CubeMesh->GetIndexBuffer(), RHI::IndexType::U32);

				const IrradiancePush push{.ViewProjection = s_CaptureProj * s_CaptureViews[face], .SkyboxIndex = skyboxIndex};
				cmd.PushConstants(&push, sizeof(push));

				cmd.DrawIndexed(submesh.IndexCount, 1, submesh.BaseIndex, (int32_t)submesh.BaseVertex);
				cmd.EndPass();
			}

			for (uint32_t mip = 0; mip < kPrefilterMips; mip++)
			{
				const float roughness = (float)mip / (float)(kPrefilterMips - 1);

				for (uint32_t face = 0; face < 6; face++)
				{
					cmd.BeginPass({.DebugName = s_PrefilterPassNames[mip],
						.Target = environment->m_Prefilter.get(),
						.Layer = face,
						.Mip = mip,
						.Color = {{.Load = RHI::LoadOp::Clear, .ClearValue = {0.0f, 0.0f, 0.0f, 1.0f}}}});

					cmd.BindPipeline(*s_PrefilterPipeline);
					cmd.BindVertexBuffer(*s_CubeMesh->GetVertexBuffer());
					cmd.BindIndexBuffer(*s_CubeMesh->GetIndexBuffer(), RHI::IndexType::U32);

					const PrefilterPush push{.ViewProjection = s_CaptureProj * s_CaptureViews[face],
						.SkyboxIndex = skyboxIndex,
						.Roughness = roughness,
						.EnvResolution = (float)kSkyboxSize};

					cmd.PushConstants(&push, sizeof(push));

					cmd.DrawIndexed(submesh.IndexCount, 1, submesh.BaseIndex, (int32_t)submesh.BaseVertex);
					cmd.EndPass();
				}
			}
		});

		TF_CORE_INFO("Baked environment skybox: {0}x{0}, 6 faces, ({1} mips), irradiance {2}x{2}, prefilter {3}x{3} ({4} mips)", kSkyboxSize, mips, kIrradianceSize, kPrefilterSize, kPrefilterMips);
		return environment;
	}

	void Environment::ReleaseResources()
	{
		s_PrefilterPipeline.reset();
		s_PrefilterShader.reset();
		s_IrradiancePipeline.reset();
		s_IrradianceShader.reset();
		s_EquirectPipeline.reset();
		s_EquirectShader.reset();
		s_CubeMesh.reset();
	}

	const Ref<RHI::Texture>& Environment::GetSkyboxMap() const
	{
		return m_Skybox->GetColor(0);
	}

	const Ref<RHI::Texture>& Environment::GetIrradianceMap() const
	{
		return m_Irradiance->GetColor(0);
	}

	const Ref<RHI::Texture>& Environment::GetPrefilterMap() const
	{
		return m_Prefilter->GetColor(0);
	}

	bool Environment::IsValid() const
	{
		return m_Skybox && m_Skybox->IsValid() && m_Irradiance && m_Irradiance->IsValid() && m_Prefilter && m_Prefilter->IsValid();
	}
}
