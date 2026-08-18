#include "tfpch.h"
#include "ImGuiTextures.h"

#include "Timefall/Renderer/Texture.h"
#include "Timefall/RHI/Texture.h"

#include <imgui_impl_vulkan.h>

namespace Timefall::UI
{
	static bool s_BackendAlive = false;

	static void DestroyTextureID(void* handle)
	{
		if (s_BackendAlive && handle)
			ImGui_ImplVulkan_RemoveTexture((VkDescriptorSet)handle);
	}

	void SetBackendAlive(bool alive)
	{
		s_BackendAlive = alive;
	}

	ImTextureID GetTextureID(const Ref<Texture2D>& texture)
	{
		if (!s_BackendAlive || !texture || !texture->IsValid())
			return 0;

		const Ref<RHI::Texture>& rhi = texture->GetRHITexture();

		if (void* cached = rhi->GetUIHandle())
			return (ImTextureID)(uint64_t)cached;

		VkDescriptorSet set = ImGui_ImplVulkan_AddTexture((VkImageView)rhi->GetNativeView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		if (!set)
			return 0;

		rhi->SetUIHandle(set, &DestroyTextureID);
		return (ImTextureID)(uint64_t)set;
	}

	void Image(ImTextureID texture, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1)
	{
		if (!texture)
		{
			ImGui::Dummy(size);
			return;
		}

		ImGui::Image(texture, size, uv0, uv1);
	}

	bool ImageButton(const char* strId, ImTextureID texture, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1,
		const ImVec4& bgColor, const ImVec4& tintColor)
	{
		if (!texture)
		{
			// Matches ImageButton's own framing so the fallback keeps the layout and stays clickable.
			const ImVec2 padding = ImGui::GetStyle().FramePadding;
			return ImGui::InvisibleButton(strId, {size.x + padding.x * 2.0f, size.y + padding.y * 2.0f});
		}

		return ImGui::ImageButton(strId, texture, size, uv0, uv1, bgColor, tintColor);
	}
}
