#include "tfpch.h"

#include "Timefall/Renderer/Renderer2D.h"

#include "Timefall/Renderer/Shader.h"
#include "Timefall/Renderer/ShaderLibrary.h"
#include "Timefall/Renderer/MSDFData.h"

#include "Timefall/Asset/AssetManager.h"

#include "Timefall/RHI/Bindings.h"
#include "Timefall/RHI/CommandList.h"
#include "Timefall/RHI/FrameAllocator.h"
#include "Timefall/RHI/GpuBuffer.h"
#include "Timefall/RHI/Pipeline.h"
#include "Timefall/RHI/RenderDevice.h"

#include <glm/gtc/matrix_transform.hpp>

namespace Timefall
{
	struct QuadVertex
	{
		glm::vec3 Position;
		glm::vec4 Color;
		glm::vec2 TexCoords;
		uint32_t TextureIndex;
		float TilingFactor;

		// Editor Only
		int EntityID;
	};

	struct CircleVertex
	{
		glm::vec3 WorldPosition;
		glm::vec2 LocalPosition;
		glm::vec4 Color;
		float Thickness;
		float Fade;

		// Editor Only
		int EntityID;
	};

	struct LineVertex
	{
		glm::vec3 Position;
		glm::vec4 Color;

		// Editor Only
		int EntityID;
	};

	struct TextVertex
	{
		glm::vec3 Position;
		glm::vec4 Color;
		glm::vec2 TexCoords;
		uint32_t AtlasIndex;

		// TODO: bg color for outline/bg

		// Editor Only
		int EntityID;
	};

	struct Renderer2DData
	{
		static constexpr uint32_t MaxQuads = 20000;
		static constexpr uint32_t MaxVertices = MaxQuads * 4;
		static constexpr uint32_t MaxIndices = MaxQuads * 6;

		Ref<RHI::GpuBuffer> QuadIndexBuffer;

		// Quads
		Ref<Shader> QuadShader;
		Ref<RHI::GraphicsPipeline> QuadPipeline;
		uint32_t QuadIndexCount = 0;
		QuadVertex* QuadVertexBufferBase = nullptr;
		QuadVertex* QuadVertexBufferPtr = nullptr;

		// Circles
		Ref<Shader> CircleShader;
		Ref<RHI::GraphicsPipeline> CirclePipeline;
		uint32_t CircleIndexCount = 0;
		CircleVertex* CircleVertexBufferBase = nullptr;
		CircleVertex* CircleVertexBufferPtr = nullptr;

		// Lines
		Ref<Shader> LineShader;
		Ref<RHI::GraphicsPipeline> LinePipeline;
		uint32_t LineVertexCount = 0;
		LineVertex* LineVertexBufferBase = nullptr;
		LineVertex* LineVertexBufferPtr = nullptr;
		float LineWidth = 2.0f;

		// Texts
		Ref<Shader> TextShader;
		Ref<RHI::GraphicsPipeline> TextPipeline;
		uint32_t TextIndexCount = 0;
		TextVertex* TextVertexBufferBase = nullptr;
		TextVertex* TextVertexBufferPtr = nullptr;

		Ref<RHI::RenderTarget> Target;
		bool ClearTarget = false;
		bool TargetFormatWarned = false;
		bool NoTargetWarned = false;

		glm::mat4 ViewProjection{1.0f};
		glm::vec4 QuadVertexPositions[4];

		Renderer2D::Statistics Stats;
	};
	static Renderer2DData s_Data;

	namespace
	{
		constexpr RHI::Format kColorFormat = RHI::Format::RGBA8Unorm;
		constexpr RHI::Format kIDFormat = RHI::Format::R32I;
		constexpr RHI::Format kDepthFormat = RHI::Format::D32F;

		constexpr glm::vec2 kDefaultTexCoords[4]{{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}};

		glm::mat4 MakeTransform(const glm::vec3& position, float rotation, const glm::vec2& size)
		{
			return glm::translate(glm::mat4(1.0f), position) * glm::rotate(glm::mat4(1.0f), glm::radians(rotation), {0.0f, 0.0f, 1.0f})
				* glm::scale(glm::mat4(1.0f), {size.x, size.y, 1.0f});
		}

		uint32_t BindlessIndexOf(const Ref<Texture2D> texture)
		{
			if (texture && texture->GetRHITexture())
				return texture->GetRHITexture()->GetBindlessIndex(false);

			return RHI::Bindings::GetWhiteTextureIndex();
		}

		Ref<RHI::GraphicsPipeline> CreatePipeline(
			const Ref<Shader>& shader, const BufferLayout& layout, RHI::Topology topology, const char* name)
		{
			RHI::GraphicsPipelineDesc desc;
			desc.ShaderModule = shader;
			desc.VertexLayout = layout;
			desc.Primitive = topology;
			desc.ColorFormats[0] = kColorFormat;
			desc.ColorFormats[1] = kIDFormat;
			desc.ColorCount = 2;
			desc.DepthFormat = kDepthFormat;
			desc.Depth = {.Test = false, .Write = false};
			desc.Blend = RHI::BlendMode::Alpha;
			desc.Raster.Cull = RHI::CullMode::None;
			desc.DebugName = name;
			return RHI::GraphicsPipeline::Create(desc);
		}
	}

	void Renderer2D::Init()
	{
		TF_PROFILE_FUNCTION();

		s_Data.QuadVertexBufferBase = new QuadVertex[Renderer2DData::MaxVertices];
		s_Data.CircleVertexBufferBase = new CircleVertex[Renderer2DData::MaxVertices];
		s_Data.LineVertexBufferBase = new LineVertex[Renderer2DData::MaxVertices];
		s_Data.TextVertexBufferBase = new TextVertex[Renderer2DData::MaxVertices];

		std::vector<uint32_t> indcies(Renderer2DData::MaxIndices);
		for (uint32_t i = 0, offset = 0; i < Renderer2DData::MaxIndices; i += 6, offset += 4)
		{
			indcies[i + 0] = offset + 0;
			indcies[i + 1] = offset + 1;
			indcies[i + 2] = offset + 2;
			indcies[i + 3] = offset + 2;
			indcies[i + 4] = offset + 3;
			indcies[i + 5] = offset + 0;
		}

		s_Data.QuadIndexBuffer = RHI::GpuBuffer::CreateWithData(
			{.Size = indcies.size() * sizeof(uint32_t), .Usage = RHI::BufferUsage::Index, .DebugName = "Renderer2DIndcies"},
			indcies.data());

		s_Data.QuadShader = ShaderLibrary::Load("Assets/Shaders/Renderer2D_Quad.slang");
		s_Data.QuadPipeline = CreatePipeline(s_Data.QuadShader,
			{{ShaderDataType::Float3, "a_Position"}, {ShaderDataType::Float4, "a_Color"},
				{ShaderDataType::Float2, "a_TexCoord"}, {ShaderDataType::UInt, "a_TexIndex"},
				{ShaderDataType::Float, "a_TilingFactor"}, {ShaderDataType::Int, "a_EntityID"}},
			RHI::Topology::TriangleList, "Renderer2DQuadPipeline");

		s_Data.CircleShader = ShaderLibrary::Load("Assets/Shaders/Renderer2D_Circle.slang");
		s_Data.CirclePipeline = CreatePipeline(s_Data.CircleShader,
			{{ShaderDataType::Float3, "a_WorldPosition"}, {ShaderDataType::Float2, "a_LocalPosition"}, {ShaderDataType::Float4, "a_Color"},
				{ShaderDataType::Float, "a_Thickness"}, {ShaderDataType::Float, "a_Fade"}, {ShaderDataType::Int, "a_Entity"}},
			RHI::Topology::TriangleList, "Renderer2DCirclePipeline");

		s_Data.QuadVertexPositions[0] = {-0.5f, -0.5f, 0.0f, 1.0f};
		s_Data.QuadVertexPositions[1] = {0.5f, -0.5f, 0.0f, 1.0f};
		s_Data.QuadVertexPositions[2] = {0.5f, 0.5f, 0.0f, 1.0f};
		s_Data.QuadVertexPositions[3] = {-0.5f, 0.5f, 0.0f, 1.0f};

		StartBatch();
	}

	void Renderer2D::Shutdown()
	{
		TF_PROFILE_FUNCTION();

		delete[] s_Data.QuadVertexBufferBase;
		delete[] s_Data.CircleVertexBufferBase;
		delete[] s_Data.LineVertexBufferBase;
		delete[] s_Data.TextVertexBufferBase;

		s_Data.QuadVertexBufferBase = nullptr;
		s_Data.CircleVertexBufferBase = nullptr;
		s_Data.LineVertexBufferBase = nullptr;
		s_Data.TextVertexBufferBase = nullptr;

		s_Data.QuadIndexBuffer.reset();
		s_Data.QuadShader.reset();
		s_Data.QuadPipeline.reset();

		s_Data.CircleShader.reset();
		s_Data.CirclePipeline.reset();

		s_Data.LineShader.reset();
		s_Data.LinePipeline.reset();

		s_Data.TextShader.reset();
		s_Data.TextPipeline.reset();

		s_Data.Target.reset();
	}

	void Renderer2D::SetTargetRenderTarget(const Ref<RHI::RenderTarget>& target, bool clear)
	{
		s_Data.Target = target;
		s_Data.ClearTarget = clear;

		if (target && target->IsValid() && !s_Data.TargetFormatWarned
			&& (target->GetColorCount() < 2 || target->GetColorFormat(0) != kColorFormat || target->GetColorFormat(1) != kIDFormat || target->GetDepthFormat() != kDepthFormat))
		{
			TF_CORE_ERROR("Renderer2D target formats do not match its pipelines (expects RGBA8Unorm + R32I + D32F)");
			s_Data.TargetFormatWarned = true;
		}
	}

	void Renderer2D::BeginScene(const Camera& camera, const glm::mat4& transform)
	{
		TF_PROFILE_FUNCTION();

		s_Data.ViewProjection = camera.GetProjection() * glm::inverse(transform);
		StartBatch();
	}

	void Renderer2D::BeginScene(const OrthographicCamera& camera)
	{
		TF_PROFILE_FUNCTION();

		s_Data.ViewProjection = camera.GetViewProjection();
		StartBatch();
	}

	void Renderer2D::BeginScene(const EditorCamera& camera)
	{
		TF_PROFILE_FUNCTION();

		s_Data.ViewProjection = camera.GetViewProjection();
		StartBatch();
	}

	void Renderer2D::EndScene()
	{
		TF_PROFILE_FUNCTION();

		if (!s_Data.Target || !s_Data.Target->IsValid())
		{
			if (!s_Data.NoTargetWarned)
			{
				TF_CORE_WARN("Renderer2D::EndScene with no render target - call SetTargetRenderTarget first");
				s_Data.NoTargetWarned = true;
			}

			StartBatch();
			return;
		}

		RHI::CommandList* cmd = RHI::RenderDevice::Get().GetCurrentCommandList();
		if (!cmd)
		{
			StartBatch();
			return;
		}

		const RHI::LoadOp load = s_Data.ClearTarget ? RHI::LoadOp::Clear : RHI::LoadOp::Load;

		s_Data.ClearTarget = false;

		cmd->BeginPass({.DebugName = "Renderer2D",
			.Target = s_Data.Target.get(),
			.Color = {{.Load = load, .ClearValue = {0.1f, 0.1f, 0.1f, 1.0f}}, {.Load = load, .ClearInt = {-1, -1, -1, 0}}},
			.Depth = {.Load = load, .ClearDepth = 1.0f}});

		Flush();

		cmd->EndPass();

		StartBatch();
	}

	void Renderer2D::Flush()
	{
		RHI::CommandList* cmd = RHI::RenderDevice::Get().GetCurrentCommandList();
		if (!cmd)
			return;

		if (s_Data.QuadIndexCount)
		{
			const uint64_t size = (uint8_t*)s_Data.QuadVertexBufferPtr - (uint8_t*)s_Data.QuadVertexBufferBase;
			const RHI::FrameAllocation vertices = RHI::FrameAllocator::Allocate(size);

			if (vertices.IsValid())
			{
				std::memcpy(vertices.Mapped, s_Data.QuadVertexBufferBase, size);

				cmd->BindPipeline(*s_Data.QuadPipeline);
				cmd->PushConstants(&s_Data.ViewProjection, sizeof(glm::mat4));
				cmd->BindVertexBuffer(*vertices.Buffer, vertices.Offset);
				cmd->BindIndexBuffer(*s_Data.QuadIndexBuffer, RHI::IndexType::U32);
				cmd->DrawIndexed(s_Data.QuadIndexCount);

				s_Data.Stats.DrawCalls++;
			}
		}

		if (s_Data.CircleIndexCount)
		{
			const uint64_t size = (uint8_t*)s_Data.CircleVertexBufferPtr - (uint8_t*)s_Data.CircleVertexBufferBase;
			const RHI::FrameAllocation vertices = RHI::FrameAllocator::Allocate(size);

			if (vertices.IsValid())
			{
				std::memcpy(vertices.Mapped, s_Data.CircleVertexBufferBase, size);

				cmd->BindPipeline(*s_Data.CirclePipeline);
				cmd->PushConstants(&s_Data.ViewProjection, sizeof(glm::mat4));
				cmd->BindVertexBuffer(*vertices.Buffer, vertices.Offset);
				cmd->BindIndexBuffer(*s_Data.QuadIndexBuffer, RHI::IndexType::U32);
				cmd->DrawIndexed(s_Data.CircleIndexCount);

				s_Data.Stats.DrawCalls++;
			}
		}
	}

	void Renderer2D::DrawQuadInternal(
		const glm::mat4& transform, const glm::vec4& color, uint32_t textureIndex, const glm::vec2* texCoords, float tiling, int entityID)
	{
		TF_PROFILE_FUNCTION();

		if (s_Data.QuadIndexCount >= Renderer2DData::MaxIndices)
			FlushAndReset();

		for (size_t i = 0; i < 4; i++)
		{
			s_Data.QuadVertexBufferPtr->Position = transform * s_Data.QuadVertexPositions[i];
			s_Data.QuadVertexBufferPtr->Color = color;
			s_Data.QuadVertexBufferPtr->TexCoords = texCoords[i];
			s_Data.QuadVertexBufferPtr->TextureIndex = textureIndex;
			s_Data.QuadVertexBufferPtr->TilingFactor = tiling;
			s_Data.QuadVertexBufferPtr->EntityID = entityID;
			s_Data.QuadVertexBufferPtr++;
		}

		s_Data.QuadIndexCount += 6;
		s_Data.Stats.QuadCount++;
	}

	void Renderer2D::FlushAndReset()
	{
		EndScene();
	}

	void Renderer2D::StartBatch()
	{
		s_Data.QuadIndexCount = 0;
		s_Data.QuadVertexBufferPtr = s_Data.QuadVertexBufferBase;

		s_Data.CircleIndexCount = 0;
		s_Data.CircleVertexBufferPtr = s_Data.CircleVertexBufferBase;

		s_Data.LineVertexCount = 0;
		s_Data.LineVertexBufferPtr = s_Data.LineVertexBufferBase;

		s_Data.TextIndexCount = 0;
		s_Data.TextVertexBufferPtr = s_Data.TextVertexBufferBase;
	}

	void Renderer2D::DrawQuad(const glm::mat4& transform, const glm::vec4& color)
	{
		DrawQuadInternal(transform, color, RHI::Bindings::GetWhiteTextureIndex(), kDefaultTexCoords, 1.0f, -1);
	}

	void Renderer2D::DrawQuad(const glm::vec2& position, float rotation, const glm::vec2& size, const glm::vec4& color)
	{
		DrawQuad(glm::vec3(position, 0.0f), rotation, size, color);
	}

	void Renderer2D::DrawQuad(const glm::vec3& position, float rotation, const glm::vec2& size, const glm::vec4& color)
	{
		DrawQuad(MakeTransform(position, rotation, size), color);
	}

	void Renderer2D::DrawQuad(const Ref<Texture2D>& texture, const glm::vec2& position, float rotation, const glm::vec2& size,
		const glm::vec4& tintColor, float tiling)
	{
		DrawQuad(texture, glm::vec3(position, 0.0f), rotation, size, tintColor, tiling);
	}

	void Renderer2D::DrawQuad(const Ref<Texture2D>& texture, const glm::mat4& transform, const glm::vec4& tintColor, float tiling)
	{
		DrawQuadInternal(transform, tintColor, BindlessIndexOf(texture), kDefaultTexCoords, tiling, -1);
	}

	void Renderer2D::DrawQuad(const Ref<Texture2D>& texture, const glm::vec3& position, float rotation, const glm::vec2& size,
		const glm::vec4& tintColor, float tiling)
	{
		DrawQuad(texture, MakeTransform(position, rotation, size), tintColor, tiling);
	}

	void Renderer2D::DrawQuad(const Ref<SubTexture2D>& subTexture, const glm::vec2& position, float rotation, const glm::vec2& size,
		const glm::vec4& tintColor, float tiling)
	{
		DrawQuad(subTexture, glm::vec3(position, 0.0f), rotation, size, tintColor, tiling);
	}

	void Renderer2D::DrawQuad(const Ref<SubTexture2D>& subTexture, const glm::vec3& position, float rotation, const glm::vec2& size,
		const glm::vec4& tint, float tilingFactor)
	{
		DrawQuadInternal(MakeTransform(position, rotation, size), tint, BindlessIndexOf(subTexture->GetTexture()),
			subTexture->GetTexCoords(), tilingFactor, -1);
	}

	void Renderer2D::DrawSprite(const glm::mat4& transform, SpriteRendererComponent src, int entityID)
	{
		TF_PROFILE_FUNCTION();

		uint32_t textureIndex = RHI::Bindings::GetWhiteTextureIndex();
		if (src.Texture)
			textureIndex = BindlessIndexOf(AssetManager::GetAsset<Texture2D>(src.Texture));

		DrawQuadInternal(transform, src.Color, textureIndex, kDefaultTexCoords, src.TilingFactor, entityID);
	}

	void Renderer2D::DrawCircle(const glm::mat4& transform, const glm::vec4& color, float thickness, float fade, int entityID)
	{
		TF_PROFILE_FUNCTION();

		if (s_Data.CircleIndexCount >= Renderer2DData::MaxIndices)
			FlushAndReset();

		for (size_t i = 0; i < 4; i++)
		{
			s_Data.CircleVertexBufferPtr->WorldPosition = transform * s_Data.QuadVertexPositions[i];
			s_Data.CircleVertexBufferPtr->LocalPosition = s_Data.QuadVertexPositions[i] * 2.0f;
			s_Data.CircleVertexBufferPtr->Color = color;
			s_Data.CircleVertexBufferPtr->Thickness = thickness;
			s_Data.CircleVertexBufferPtr->Fade = fade;
			s_Data.CircleVertexBufferPtr->EntityID = entityID;
			s_Data.CircleVertexBufferPtr++;
		}

		s_Data.CircleIndexCount += 6;
		s_Data.Stats.CircleCount++;
	}

	void Renderer2D::DrawLine(const glm::vec3& p0, const glm::vec3& p1, const glm::vec4& color, int entityID) {}

	void Renderer2D::DrawRect(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color, int entityID) {}

	void Renderer2D::DrawRect(const glm::mat4& transform, const glm::vec4& color, int entityID) {}

	void Renderer2D::DrawString(
		const std::string& text, const Ref<Font>& font, const glm::mat4& transform, const TextParams& params, int entityID)
	{}

	void Renderer2D::DrawString(const std::string& text, const glm::mat4& transform, const TextComponent& component, int entityID) {}

	float Renderer2D::GetLineWidth()
	{
		return s_Data.LineWidth;
	}

	void Renderer2D::SetLineWidth(float width)
	{
		s_Data.LineWidth = width;
	}

	Renderer2D::Statistics& Renderer2D::GetStats()
	{
		return s_Data.Stats;
	}

	void Renderer2D::ResetStats()
	{
		s_Data.Stats = {};
	}
}
