#include "tfpch.h"
#include "Timefall/ImGui/ImGuiLayer.h"
#include "Timefall/Core/Application.h"

#include "Platform/Vulkan/VulkanContext.h"
#include "Timefall/RHI/RenderDevice.h"
#include "Timefall/RHI/CommandList.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include "ImGuizmo.h"

namespace Timefall
{
	ImGuiLayer::ImGuiLayer()
		: Layer("ImGuiLayer")
	{}

	ImGuiLayer::~ImGuiLayer() = default;

	void ImGuiLayer::OnAttach()
	{
		TF_PROFILE_FUNCTION();

		// Setup Dear ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		(void)io;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
		// io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // Enable Docking
		// io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // Enable Multi-Viewport / Platform Windows
		//  io.ConfigFlags |= ImGuiConfigFlags_ViewportsNoTaskBarIcons;
		//  io.ConfigFlags |= ImGuiConfigFlags_ViewportsNoMerge;

		io.Fonts->AddFontFromFileTTF("Assets/Fonts/OpenSans/static/OpenSans-Bold.ttf", 18.0f);
		io.FontDefault = io.Fonts->AddFontFromFileTTF("Assets/Fonts/OpenSans/static/OpenSans-Regular.ttf", 18.0f);

		// Setup Dear ImGui style
		ImGui::StyleColorsDark();
		// ImGui::StyleColorsClassic();

		SetDarkThemeColors();

		// When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
		ImGuiStyle& style = ImGui::GetStyle();
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			style.WindowRounding = 0.0f;
			style.Colors[ImGuiCol_WindowBg].w = 1.0f;
		}

		Application& app = Application::Get();
		GLFWwindow* window = static_cast<GLFWwindow*>(app.GetWindow().GetNativeWindow());

		auto& ctx = VulkanContext::Get();
		auto& renderDeivce = RHI::RenderDevice::Get();

		constexpr uint32_t maxTextures = 1000;

		const vk::DescriptorPoolSize poolSizes[]{
			{vk::DescriptorType::eSampledImage, maxTextures},
			{vk::DescriptorType::eSampler, IMGUI_IMPL_VULKAN_MINIMUM_SAMPLER_POOL_SIZE},
		};

		auto pool = ctx.GetDevice().createDescriptorPool({.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
			.maxSets = maxTextures + IMGUI_IMPL_VULKAN_MINIMUM_SAMPLER_POOL_SIZE,
			.poolSizeCount = (uint32_t)std::size(poolSizes),
			.pPoolSizes = poolSizes});

		if (!pool)
		{
			TF_CORE_ERROR("createDescriptorPool failed: {0}", vk::to_string(pool.error()));
			return;
		}
		m_ImGuiPool = *pool;

		static vk::Instance s_Instance = ctx.GetInstance();

		ImGui_ImplVulkan_LoadFunctions(
			VK_API_VERSION_1_4,
			[](const char* name, void* user) {
				auto instance = *static_cast<vk::Instance*>(user);
				return VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr(instance, name);
			},
			&s_Instance);

		static VkFormat colorFormat = (VkFormat)renderDeivce.GetSwapchainFormat();

		VkPipelineRenderingCreateInfo rendering{};
		rendering.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
		rendering.colorAttachmentCount = 1;
		rendering.pColorAttachmentFormats = &colorFormat;

		ImGui_ImplVulkan_InitInfo init{};
		init.Instance = s_Instance;
		init.PhysicalDevice = ctx.GetPhysicalDevice();
		init.Device = ctx.GetDevice();
		init.QueueFamily = ctx.GetGraphicsQueueFamily();
		init.Queue = ctx.GetGraphicsQueue();
		init.DescriptorPool = m_ImGuiPool;
		init.MinImageCount = renderDeivce.GetSwapchainMinImageCount();
		init.ImageCount = renderDeivce.GetSwapchainImageCount();
		init.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
		init.PipelineInfoMain.PipelineRenderingCreateInfo = rendering;
		init.UseDynamicRendering = true;
		init.MinAllocationSize = 1024 * 1024; // matches the VMA sub-allocation threshold; silences "Perf" warnings
		init.CheckVkResultFn = [](VkResult err) {
			if (err != VK_SUCCESS)
				TF_CORE_ERROR("[ImGui-Vulkan] {0}", vk::to_string((vk::Result)err));
		};

		ImGui_ImplGlfw_InitForVulkan(window, true);
		ImGui_ImplVulkan_Init(&init);
	}

	void ImGuiLayer::OnDetach()
	{
		TF_PROFILE_FUNCTION();

		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		VulkanContext::Get().GetDevice().destroyDescriptorPool(m_ImGuiPool);
		ImGui::DestroyContext();
	}

	void ImGuiLayer::Begin()
	{
		TF_PROFILE_FUNCTION();

		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
		ImGuizmo::BeginFrame();
	}

	void ImGuiLayer::End()
	{
		TF_PROFILE_FUNCTION();

		ImGui::Render();

		if (auto* cmd = RHI::RenderDevice::Get().GetCurrentCommandList())
			ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), (VkCommandBuffer)cmd->GetNativeHandle());
	}

	void ImGuiLayer::OnEvent(Event& e)
	{
		if (m_BlockEvents)
		{
			ImGuiIO& io = ImGui::GetIO();
			e.Handled |= e.IsInCategory(EventCategoryMouse) & io.WantCaptureMouse;
			e.Handled |= e.IsInCategory(EventCategoryKeyboard) & io.WantCaptureKeyboard;
		}

		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<WindowResizeEvent>(TF_BIND_EVENT_FN(ImGuiLayer::OnWindowResize));
	}

	void ImGuiLayer::SetDarkThemeColors()
	{
		auto& colors = ImGui::GetStyle().Colors;
		colors[ImGuiCol_WindowBg] = ImVec4(0.1f, 0.105f, 0.11f, 1.0f);

		// Headers
		colors[ImGuiCol_Header] = ImVec4(0.2f, 0.205f, 0.21f, 1.0f);
		colors[ImGuiCol_HeaderHovered] = ImVec4(0.3f, 0.305f, 0.31f, 1.0f);
		colors[ImGuiCol_HeaderActive] = ImVec4(0.15f, 0.1505f, 0.151f, 1.0f);

		// Buttons
		colors[ImGuiCol_Button] = ImVec4(0.2f, 0.205f, 0.21f, 1.0f);
		colors[ImGuiCol_ButtonHovered] = ImVec4(0.3f, 0.305f, 0.31f, 1.0f);
		colors[ImGuiCol_ButtonActive] = ImVec4(0.15f, 0.1505f, 0.151f, 1.0f);

		// Frame BG
		colors[ImGuiCol_FrameBg] = ImVec4(0.2f, 0.205f, 0.21f, 1.0f);
		colors[ImGuiCol_FrameBgHovered] = ImVec4(0.3f, 0.305f, 0.31f, 1.0f);
		colors[ImGuiCol_FrameBgActive] = ImVec4(0.15f, 0.1505f, 0.151f, 1.0f);

		// Tabs
		colors[ImGuiCol_Tab] = ImVec4(0.15f, 0.1505f, 0.151f, 1.0f);
		colors[ImGuiCol_TabHovered] = ImVec4(0.38f, 0.3805f, 0.381f, 1.0f);
		colors[ImGuiCol_TabSelected] = ImVec4(0.28f, 0.2805f, 0.281f, 1.0f);
		colors[ImGuiCol_TabDimmed] = ImVec4(0.15f, 0.1505f, 0.151f, 1.0f);
		colors[ImGuiCol_TabDimmedSelected] = ImVec4(0.2f, 0.205f, 0.21f, 1.0f);

		// Title
		colors[ImGuiCol_TitleBg] = ImVec4(0.15f, 0.1505f, 0.151f, 1.0f);
		colors[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.1505f, 0.151f, 1.0f);
		colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.15f, 0.1505f, 0.151f, 1.0f);
	}

	uint32_t ImGuiLayer::GetActiveWidgetID() const
	{
		return GImGui->ActiveId;
	}

	bool ImGuiLayer::OnWindowResize(WindowResizeEvent& e)
	{
		const uint32_t minImageCount = RHI::RenderDevice::Get().GetSwapchainMinImageCount();
		if (minImageCount >= 2)
			ImGui_ImplVulkan_SetMinImageCount(minImageCount);

		return false;
	}
}
