#include "SceneHierarchyPanel.h"

#include <imgui/imgui.h>

#include "Timefall/Scene/Components.h"

namespace Timefall
{
	SceneHierarchyPanel::SceneHierarchyPanel(const Ref<Scene>& context)
	{
		SetContext(context);
	}

	void SceneHierarchyPanel::OnImGuiRender()
	{
		ImGui::Begin("Scene Hierarchy");

		m_Context->m_Registry.view<entt::entity>().each([&](auto entity)
		{
			Entity e = { entity, m_Context.get() };
			DrawEntityNode(e);
		});

		ImGui::End();
	}

	void SceneHierarchyPanel::DrawEntityNode(Entity entity)
	{
		auto& tag = entity.GetComponent<TagComponent>().Tag;

		ImGuiTreeNodeFlags treeNodeFlags = ((m_SelectionContext == entity) ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow;
		bool opened = ImGui::TreeNodeEx((void*)(uint64_t)(uint32_t)entity, treeNodeFlags, tag.c_str());

		if (ImGui::IsItemClicked())
			m_SelectionContext = entity;

		if (opened)
			ImGui::TreePop();
	}
}