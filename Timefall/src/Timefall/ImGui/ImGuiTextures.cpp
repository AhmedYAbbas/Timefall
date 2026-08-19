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

	ImTextureID GetTextureID(const Ref<RHI::Texture>& texture)
	{
		if (!s_BackendAlive || !texture || !texture->IsValid())
			return 0;

		if (void* cached = texture->GetUIHandle())
			return (ImTextureID)(uint64_t)cached;

		VkDescriptorSet set = ImGui_ImplVulkan_AddTexture((VkImageView)texture->GetNativeView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		if (!set)
			return 0;

		texture->SetUIHandle(set, &DestroyTextureID);
		return (ImTextureID)(uint64_t)set;
	}

	ImTextureID GetTextureID(const Ref<Texture2D>& texture)
	{
		if (!texture || !texture->IsValid())
			return 0;

		return GetTextureID(texture->GetRHITexture());
	}
}
