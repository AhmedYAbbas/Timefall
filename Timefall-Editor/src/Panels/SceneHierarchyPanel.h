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

		void SetContext(const Ref<Scene> context) { m_Context = context; }

		void OnImGuiRender();

	private:
		void DrawEntityNode(Entity entity);
		void DrawComponents(Entity entity);

	private:
		Ref<Scene> m_Context;
		Entity m_SelectionContext;
	};
}