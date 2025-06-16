#pragma once

#include "Timefall/Core/Log.h"
#include "Timefall/Core/Core.h"
#include "Timefall/Scene/Scene.h"
#include "Timefall/Scene/Entity.h"

namespace Timefall
{
	class SceneHierarchyPanel
	{
	public:
		SceneHierarchyPanel() = default;
		SceneHierarchyPanel(const Ref<Scene>& context);

		void SetContext(const Ref<Scene> context);

		void OnImGuiRender();

		Entity GetSelectedEntity() const { return m_SelectionContext; }
		void SetSelectedEntity(Entity entity) { m_SelectionContext = entity; }

	private:
		void DrawEntityNode(Entity entity);
		void DrawComponents(Entity entity);

		template<typename T>
		void DisplayAddComponentEntry(const std::string& entryName);

	private:
		Ref<Scene> m_Context;
		Entity m_SelectionContext;
	};
}