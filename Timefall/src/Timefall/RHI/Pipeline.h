#pragma once

#include "Timefall/Core/Core.h"
#include "Timefall/RHI/RHITypes.h"
#include "Timefall/Renderer/Buffer.h"
#include "Timefall/Renderer/Shader.h"

namespace Timefall::RHI
{
	enum class CompareOp : uint8_t { Never = 0, Less, Equal, LessOrEqual, Greater, NotEqual, GreaterOrEqual, Always };

	enum class CullMode : uint8_t { None = 0, Front, Back };

	enum class BlendMode : uint8_t { None = 0, Alpha, Additive };

	enum class Topology : uint8_t { TriangleList, LineList };

	struct DepthState
	{
		bool Test = false;
		bool Write = false;
		CompareOp Compare = CompareOp::LessOrEqual;
	};

	struct RasterState
	{
		CullMode Cull = CullMode::Back;
		bool FrontFaceCCW = true;
	};

	struct GraphicsPipelineDesc
	{
		Ref<Shader> ShaderModule;
		BufferLayout VertexLayout; // empty == no vertex buffers (fullscreen / procedural draws)
		Topology Primitive = Topology::TriangleList;
		Format ColorFormats[8]{};
		uint32_t ColorCount = 0;
		Format DepthFormat = Format::Undefined;
		DepthState Depth{};
		BlendMode Blend = BlendMode::None;
		RasterState Raster{};
		const char* DebugName = nullptr;
	};

	class TF_API GraphicsPipeline
	{
	public:
		static Ref<GraphicsPipeline> Create(const GraphicsPipelineDesc& desc);

		~GraphicsPipeline();

		GraphicsPipeline(const GraphicsPipeline&) = delete;
		GraphicsPipeline& operator=(const GraphicsPipeline&) = delete;

		bool Recreate();

		const Ref<Shader>& GetShader() const;
		bool IsValid() const;

	private:
		GraphicsPipeline() = default;

		bool Build();

		friend class CommandList;

		struct Impl;
		Impl* m_Impl = nullptr;
	};
}
