#pragma once

#include "Timefall/Core/Log.h"
#include "Timefall/Core/Core.h"
#include "Timefall/Core/UUID.h"
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

		template <typename T> void DisplayAddComponentEntry(const std::string& entryName);

		// Applies the pending reparent / create-child / delete captured during the tree draw.
		// Deferred so structural edits never mutate the registry mid-iteration.
		void ApplyDeferredEdits();

	private:
		Ref<Scene> m_Context;
		Entity m_SelectionContext;

		// Deferred structural edits (UUID 0 = "none"; reparent uses an explicit flag since
		// parent 0 is a valid request meaning "unparent").
		bool m_PendingReparent = false;
		UUID m_PendingReparentChild = 0;
		UUID m_PendingReparentParent = 0;
		UUID m_PendingCreateChildParent = 0;
		UUID m_PendingDelete = 0;
	};
}