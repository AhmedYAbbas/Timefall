#include "tfpch.h"

#include "Timefall/RHI/Pipeline.h"
#include "Timefall/RHI/RenderDevice.h"

#include "Platform/Vulkan/VulkanContext.h"
#include "Platform/Vulkan/VulkanRHIImpl.h"
#include "Platform/Vulkan/VulkanBindings.h"

namespace Timefall::RHI
{
	static vk::Format ToVkFormat(ShaderDataType type)
	{
		switch (type)
		{
			case ShaderDataType::Float: return vk::Format::eR32Sfloat;
			case ShaderDataType::Float2: return vk::Format::eR32G32Sfloat;
			case ShaderDataType::Float3: return vk::Format::eR32G32B32Sfloat;
			case ShaderDataType::Float4: return vk::Format::eR32G32B32A32Sfloat;
			case ShaderDataType::UInt: return vk::Format::eR32Uint;
			case ShaderDataType::Int: return vk::Format::eR32Sint;
			case ShaderDataType::Int2: return vk::Format::eR32G32Sint;
			case ShaderDataType::Int3: return vk::Format::eR32G32B32Sint;
			case ShaderDataType::Int4: return vk::Format::eR32G32B32A32Sint;
			default: return vk::Format::eUndefined;
		}
	}

	static vk::CompareOp ToVkCompareOp(CompareOp op)
	{
		switch (op)
		{
			case CompareOp::Never: return vk::CompareOp::eNever;
			case CompareOp::Less: return vk::CompareOp::eLess;
			case CompareOp::Equal: return vk::CompareOp::eEqual;
			case CompareOp::LessOrEqual: return vk::CompareOp::eLessOrEqual;
			case CompareOp::Greater: return vk::CompareOp::eGreater;
			case CompareOp::NotEqual: return vk::CompareOp::eNotEqual;
			case CompareOp::GreaterOrEqual: return vk::CompareOp::eGreaterOrEqual;
			default: return vk::CompareOp::eAlways;
		}
	}

	static vk::ShaderModule CreateModule(vk::Device device, std::span<const uint32_t> spirv, const std::string& debugName)
	{
		auto module = device.createShaderModule({.codeSize = spirv.size() * sizeof(uint32_t), .pCode = spirv.data()});
		if (!module)
		{
			TF_CORE_ERROR("createShaderModule failed for '{0}': {1}", debugName, vk::to_string(module.error()));
			return nullptr;
		}

		VulkanContext::Get().SetObjectName(*module, debugName);
		return *module;
	}

	static void RetirePipeline(vk::Pipeline pipeline)
	{
		if (!pipeline)
			return;

		RenderDevice::Get().DeferDestroy([pipeline]() { VulkanContext::Get().GetDevice().destroyPipeline(pipeline); });
	}

	bool GraphicsPipeline::Build()
	{
		Impl& impl = *m_Impl;

		auto& ctx = VulkanContext::Get();
		auto device = ctx.GetDevice();
		const auto& desc = impl.Desc;
		const Ref<Shader>& shader = desc.ShaderModule;

		if (!shader || !shader->IsValid())
			return false;

		if (!shader->HasStage(ShaderStage::Vertex) || !shader->HasStage(ShaderStage::Fragment))
		{
			TF_CORE_ERROR("Shader '{0}' needs both a vertex and a fragment entry point", shader->GetName());
			return false;
		}

		const ShaderReflection& reflection = shader->GetReflection();
		if (reflection.VertexInputs.size() != desc.VertexLayout.GetElements().size())
		{
			TF_CORE_ERROR("Shader '{0}' declares {1} vertex inputs but the layout supplies {2}", shader->GetName(),
				reflection.VertexInputs.size(), desc.VertexLayout.GetElements().size());
			return false;
		}

		// The shader's own name beats a synthesized one when the pipeline was declared without a name.
		const std::string name = desc.DebugName ? desc.DebugName : std::format("Pipeline[{}]", shader->GetName());

		impl.Layout = VulkanBindings::GetLayoutFor(reflection, name);
		if (!impl.Layout)
			return false;

		impl.HasGlobalPrefix = true;
		impl.PushStages = VulkanBindings::kAllStages;
		impl.PushSize = reflection.PushConstantSize;

		const vk::ShaderModule vertexModule = CreateModule(device, shader->GetSpirv(ShaderStage::Vertex), std::format("{}:VS", name));
		const vk::ShaderModule fragmentModule = CreateModule(device, shader->GetSpirv(ShaderStage::Fragment), std::format("{}:FS", name));
		if (!vertexModule || !fragmentModule)
		{
			if (vertexModule)
				device.destroyShaderModule(vertexModule);
			if (fragmentModule)
				device.destroyShaderModule(fragmentModule);
			return false;
		}

		const std::string& vertexEntry = shader->GetEntryPointName(ShaderStage::Vertex);
		const std::string& fragmentEntry = shader->GetEntryPointName(ShaderStage::Fragment);

		const vk::PipelineShaderStageCreateInfo stages[]{
			{.stage = vk::ShaderStageFlagBits::eVertex, .module = vertexModule, .pName = vertexEntry.c_str()},
			{.stage = vk::ShaderStageFlagBits::eFragment, .module = fragmentModule, .pName = fragmentEntry.c_str()}};

		std::vector<vk::VertexInputAttributeDescription> attributes;
		uint32_t location = 0;
		for (const auto& element : desc.VertexLayout.GetElements())
		{
			const vk::Format format = ToVkFormat(element.Type);
			if (format == vk::Format::eUndefined)
			{
				TF_CORE_ERROR("Vertex attribute '{0}' has an unsupported type", element.Name);

				device.destroyShaderModule(vertexModule);
				device.destroyShaderModule(fragmentModule);
				return false;
			}

			attributes.push_back({.location = location++, .binding = 0, .format = format, .offset = (uint32_t)element.Offset});
		}

		const vk::VertexInputBindingDescription vertexBinding{
			.binding = 0, .stride = desc.VertexLayout.GetStride(), .inputRate = vk::VertexInputRate::eVertex};

		const vk::PipelineVertexInputStateCreateInfo vertexInput{.vertexBindingDescriptionCount = attributes.empty() ? 0u : 1u,
			.pVertexBindingDescriptions = &vertexBinding,
			.vertexAttributeDescriptionCount = (uint32_t)attributes.size(),
			.pVertexAttributeDescriptions = attributes.data()};

		const vk::PipelineInputAssemblyStateCreateInfo inputAssembly{
			.topology = desc.Primitive == Topology::LineList ? vk::PrimitiveTopology::eLineList : vk::PrimitiveTopology::eTriangleList};

		const vk::PipelineViewportStateCreateInfo viewport{.viewportCount = 1, .scissorCount = 1};

		const bool isLineList = desc.Primitive == Topology::LineList;
		const vk::PipelineRasterizationLineStateCreateInfo lineState{.lineRasterizationMode = vk::LineRasterizationMode::eRectangularSmooth};

		const bool smoothLines = isLineList && VulkanContext::Get().SupportsSmoothLines();

		const vk::PipelineRasterizationStateCreateInfo raster{.pNext = smoothLines ? &lineState : nullptr, .polygonMode = vk::PolygonMode::eFill,
			.cullMode = desc.Raster.Cull == CullMode::None ? vk::CullModeFlags{}
				: desc.Raster.Cull == CullMode::Front      ? vk::CullModeFlags{vk::CullModeFlagBits::eFront}
														   : vk::CullModeFlags{vk::CullModeFlagBits::eBack},
			.frontFace = desc.Raster.FrontFaceCCW ? vk::FrontFace::eCounterClockwise : vk::FrontFace::eClockwise,
			.lineWidth = 1.0f};

		const vk::PipelineMultisampleStateCreateInfo multisample{.rasterizationSamples = vk::SampleCountFlagBits::e1};

		const vk::PipelineDepthStencilStateCreateInfo depthStencil{
			.depthTestEnable = desc.Depth.Test, .depthWriteEnable = desc.Depth.Write, .depthCompareOp = ToVkCompareOp(desc.Depth.Compare)};

		constexpr vk::ColorComponentFlags allChannels = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG
			| vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

		std::vector<vk::PipelineColorBlendAttachmentState> blendAttachments(desc.ColorCount);
		for (uint32_t i = 0; i < desc.ColorCount; i++)
		{
			auto& attachment = blendAttachments[i];

			attachment.blendEnable = desc.Blend != BlendMode::None && !IsIntegerFormat(desc.ColorFormats[i]);
			attachment.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
			attachment.dstColorBlendFactor = desc.Blend == BlendMode::Additive ? vk::BlendFactor::eOne : vk::BlendFactor::eOneMinusSrcAlpha;
			attachment.colorBlendOp = vk::BlendOp::eAdd;
			attachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;

			// Source-over on alpha, not replace: the target's coverage must survive a blended draw or
			// ImGui composites the viewport texture through the hole it left.
			attachment.dstAlphaBlendFactor
				= desc.Blend == BlendMode::Additive ? vk::BlendFactor::eOne : vk::BlendFactor::eOneMinusSrcAlpha;
			attachment.alphaBlendOp = vk::BlendOp::eAdd;
			attachment.colorWriteMask = desc.ColorWrite[i] ? allChannels : vk::ColorComponentFlags{};
		}

		const vk::PipelineColorBlendStateCreateInfo blend{
			.attachmentCount = (uint32_t)blendAttachments.size(), .pAttachments = blendAttachments.data()};

		std::vector<vk::DynamicState> dynamicStates{vk::DynamicState::eViewport, vk::DynamicState::eScissor};
		if (isLineList)
			dynamicStates.push_back(vk::DynamicState::eLineWidth);

		const vk::PipelineDynamicStateCreateInfo dynamic{
			.dynamicStateCount = (uint32_t)dynamicStates.size(), .pDynamicStates = dynamicStates.data()};

		std::array<vk::Format, 8> colorFormats{};
		for (uint32_t i = 0; i < desc.ColorCount; i++)
			colorFormats[i] = ToVkFormat(desc.ColorFormats[i]);

		vk::PipelineRenderingCreateInfo rendering{.colorAttachmentCount = desc.ColorCount,
			.pColorAttachmentFormats = colorFormats.data(),
			.depthAttachmentFormat = ToVkFormat(desc.DepthFormat)};

		auto pipeline = device.createGraphicsPipeline(nullptr,
			{.pNext = &rendering,
				.stageCount = 2,
				.pStages = stages,
				.pVertexInputState = &vertexInput,
				.pInputAssemblyState = &inputAssembly,
				.pViewportState = &viewport,
				.pRasterizationState = &raster,
				.pMultisampleState = &multisample,
				.pDepthStencilState = &depthStencil,
				.pColorBlendState = &blend,
				.pDynamicState = &dynamic,
				.layout = impl.Layout});

		device.destroyShaderModule(vertexModule);
		device.destroyShaderModule(fragmentModule);

		if (!pipeline.has_value())
		{
			TF_CORE_ERROR("createGraphicsPipeline failed for '{0}'!", name);
			return false;
		}

		impl.Pipeline = *pipeline;
		impl.BuiltRevision = shader->GetRevision();

		ctx.SetObjectName(impl.Pipeline, name);

		return true;
	}

	static std::vector<std::weak_ptr<GraphicsPipeline>> s_LivePipelines;

	Ref<GraphicsPipeline> GraphicsPipeline::Create(const GraphicsPipelineDesc& desc)
	{
		TF_PROFILE_FUNCTION();

		Ref<GraphicsPipeline> pipeline(new GraphicsPipeline());
		pipeline->m_Impl = new Impl();
		pipeline->m_Impl->Desc = desc;

		if (!pipeline->Build())
			TF_CORE_ERROR("Pipeline '{0}' failed to build", desc.DebugName ? desc.DebugName : "<unnamed>");

		s_LivePipelines.push_back(pipeline);
		return pipeline;
	}

	uint32_t GraphicsPipeline::RecreateAll()
	{
		TF_PROFILE_FUNCTION();

		RenderDevice::Get().WaitIdle();

		uint32_t rebuilt = 0;
		for (auto it = s_LivePipelines.begin(); it != s_LivePipelines.end();)
		{
			if (auto pipeline = it->lock())
			{
				rebuilt += pipeline->Recreate() ? 1 : 0;
				++it;
			}
			else
			{
				it = s_LivePipelines.erase(it);
			}
		}

		return rebuilt;
	}

	GraphicsPipeline::~GraphicsPipeline()
	{
		if (!m_Impl)
			return;

		RetirePipeline(m_Impl->Pipeline);
		delete m_Impl;
		m_Impl = nullptr;
	}

	bool GraphicsPipeline::Recreate()
	{
		if (!m_Impl || !m_Impl->Desc.ShaderModule)
			return false;

		if (m_Impl->Desc.ShaderModule->GetRevision() == m_Impl->BuiltRevision)
			return false;

		const vk::Pipeline oldPipeline = m_Impl->Pipeline;
		const vk::PipelineLayout oldLayout = m_Impl->Layout;

		m_Impl->Pipeline = nullptr;
		m_Impl->Layout = nullptr;

		if (!Build())
		{
			m_Impl->Pipeline = oldPipeline;
			m_Impl->Layout = oldLayout;
			return false;
		}

		RetirePipeline(oldPipeline);
		return true;
	}

	const Ref<Shader>& GraphicsPipeline::GetShader() const
	{
		return m_Impl->Desc.ShaderModule;
	}

	bool GraphicsPipeline::IsValid() const
	{
		return m_Impl && m_Impl->Pipeline;
	}
}
