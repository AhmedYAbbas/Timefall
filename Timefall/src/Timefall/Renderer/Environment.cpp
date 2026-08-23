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
	static constexpr RHI::Format kCubeFormat = RHI::Format::RGBA16F;

	static const glm::mat4 s_CaptureProj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
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

	struct EquirectPush
	{
		glm::mat4 ViewProjection{1.0f};
		uint32_t EquirectIndex = 0;
		uint32_t _Pad[3]{};
	};
	static_assert(sizeof(EquirectPush) == 80);

	static Ref<MeshSource> s_CubeMesh;
	static Ref<Shader> s_EquirectShader;
	static Ref<RHI::GraphicsPipeline> s_EquirectPipeline;

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

		return s_CubeMesh && s_CubeMesh->HasGpuBuffers() && s_EquirectPipeline && s_EquirectPipeline->IsValid();
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

		const uint32_t equirectIndex = equirect->GetRHITexture()->GetBindlessIndex(false);
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
		});

		TF_CORE_INFO("Baked environment skybox: {0}x{0}, 6 faces, {1} mips", kSkyboxSize, mips);
		return environment;
	}

	void Environment::ReleaseResources()
	{
		s_EquirectPipeline.reset();
		s_EquirectShader.reset();
		s_CubeMesh.reset();
	}

	const Ref<RHI::Texture>& Environment::GetSkyboxMap() const
	{
		return m_Skybox->GetColor(0);
	}

	bool Environment::IsValid() const
	{
		return m_Skybox && m_Skybox->IsValid();
	}
}
