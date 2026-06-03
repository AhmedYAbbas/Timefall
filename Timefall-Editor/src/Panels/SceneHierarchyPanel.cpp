#include "SceneHierarchyPanel.h"

#include "Timefall/Scene/Components.h"
#include "Timefall/Core/Input.h"
#include "Timefall/Scripting/ScriptEngine.h"
#include "Timefall/UI/UI.h"
#include "Timefall/Asset/AssetManager.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <imgui/misc/cpp/imgui_stdlib.h>

#include <glm/gtc/type_ptr.hpp>

namespace Timefall
{
	SceneHierarchyPanel::SceneHierarchyPanel(const Ref<Scene>& context)
	{
		SetContext(context);
	}

	void SceneHierarchyPanel::SetContext(const Ref<Scene> context)
	{
		m_Context = context;
		m_SelectionContext = {};
	}

	void SceneHierarchyPanel::OnImGuiRender()
	{
		ImGui::Begin("Scene Hierarchy");

		if (m_Context)
		{
			// A previously-selected entity may have been destroyed (e.g. by a script via the
			// deferred-destroy queue) while still selected, leaving a stale handle. Clear it
			// before anything below touches its components (Properties panel, etc.).
			if (m_SelectionContext && !m_SelectionContext.IsValid())
				m_SelectionContext = {};

			// Draw only roots at the top level; DrawEntityNode recurses into Children. An entity
			// whose Parent is missing (0 or a dangling UUID) is treated as a root so it stays visible.
			m_Context->m_Registry.view<entt::entity>().each([&](auto entityHandle)
			{
				Entity e = { entityHandle, m_Context.get() };

				bool isRoot = true;
				if (e.HasComponent<RelationshipComponent>())
				{
					UUID parentID = e.GetComponent<RelationshipComponent>().Parent;
					isRoot = (parentID == 0) || !m_Context->GetEntityByUUID(parentID);
				}

				if (isRoot)
					DrawEntityNode(e);
			});

			// Whole-window drop target: dropping an entity onto empty space unparents it. ImGui's
			// smallest-surface-wins target resolution means a drop onto a node still hits the node.
			if (ImGui::BeginDragDropTargetCustom(ImGui::GetCurrentWindow()->Rect(), ImGui::GetID("SceneHierarchyUnparentTarget")))
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCENE_GRAPH_ENTITY"))
				{
					m_PendingReparent = true;
					m_PendingReparentChild = *(const UUID*)payload->Data;
					m_PendingReparentParent = 0; // unparent
				}
				ImGui::EndDragDropTarget();
			}

			if (Input::IsMouseButtonPressed(MouseCode::Button0) && ImGui::IsWindowHovered())
				m_SelectionContext = {};

			// Right-click to open context menu on empty space
			if (ImGui::BeginPopupContextWindow(0, ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
			{
				if (ImGui::MenuItem("Create Empty Entity"))
				{
					m_Context->CreateEntity("Empty Entity");
				}

				ImGui::EndPopup();
			}

			// Apply captured structural edits now that the tree iteration is complete.
			ApplyDeferredEdits();
		}

		ImGui::End();

		ImGui::Begin("Properties");

		if (m_SelectionContext)
			DrawComponents(m_SelectionContext);

		ImGui::End();
	}

	void SceneHierarchyPanel::DrawEntityNode(Entity entity)
	{
		auto& tag = entity.GetComponent<TagComponent>().Tag;

		bool hasChildren = entity.HasComponent<RelationshipComponent>()
			&& !entity.GetComponent<RelationshipComponent>().Children.empty();

		ImGuiTreeNodeFlags treeNodeFlags = ((m_SelectionContext == entity) ? ImGuiTreeNodeFlags_Selected : 0)
			| ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
		if (!hasChildren)
			treeNodeFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

		bool opened = ImGui::TreeNodeEx((void*)(uint64_t)entity.GetUUID(), treeNodeFlags, tag.c_str());

		if (ImGui::IsItemClicked())
			m_SelectionContext = entity;

		// Drag source: carries this entity's UUID.
		if (ImGui::BeginDragDropSource())
		{
			UUID id = entity.GetUUID();
			ImGui::SetDragDropPayload("SCENE_GRAPH_ENTITY", &id, sizeof(UUID));
			ImGui::Text("%s", tag.c_str());
			ImGui::EndDragDropSource();
		}

		// Drop target: reparent the dropped entity under this one (deferred).
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCENE_GRAPH_ENTITY"))
			{
				m_PendingReparent = true;
				m_PendingReparentChild = *(const UUID*)payload->Data;
				m_PendingReparentParent = entity.GetUUID();
			}
			ImGui::EndDragDropTarget();
		}

		if (ImGui::BeginPopupContextItem())
		{
			if (ImGui::MenuItem("Create Child Entity"))
				m_PendingCreateChildParent = entity.GetUUID();

			if (ImGui::MenuItem("Delete Entity"))
				m_PendingDelete = entity.GetUUID();

			ImGui::EndPopup();
		}

		// Recurse into children (only parents push a tree level / need TreePop).
		if (opened && hasChildren)
		{
			// Copy: the live Children list may change via this frame's deferred edits.
			std::vector<UUID> children = entity.GetComponent<RelationshipComponent>().Children;
			for (UUID childID : children)
			{
				Entity child = m_Context->GetEntityByUUID(childID);
				if (child)
					DrawEntityNode(child);
			}

			ImGui::TreePop();
		}
	}

	void SceneHierarchyPanel::ApplyDeferredEdits()
	{
		if (m_PendingReparent)
		{
			Entity child = m_Context->GetEntityByUUID(m_PendingReparentChild);
			Entity parent = m_PendingReparentParent != 0 ? m_Context->GetEntityByUUID(m_PendingReparentParent) : Entity{};
			if (child)
				m_Context->SetParent(child, parent);

			m_PendingReparent = false;
			m_PendingReparentChild = 0;
			m_PendingReparentParent = 0;
		}

		if (m_PendingCreateChildParent != 0)
		{
			Entity parent = m_Context->GetEntityByUUID(m_PendingCreateChildParent);
			if (parent)
			{
				Entity child = m_Context->CreateEntity("Empty Entity");
				m_Context->SetParent(child, parent);
			}
			m_PendingCreateChildParent = 0;
		}

		if (m_PendingDelete != 0)
		{
			Entity toDelete = m_Context->GetEntityByUUID(m_PendingDelete);
			if (toDelete)
			{
				if (m_SelectionContext == toDelete)
					m_SelectionContext = {};
				m_Context->DestroyEntity(toDelete);
			}
			m_PendingDelete = 0;
		}
	}

	static void DrawVec3Control(const std::string& label, glm::vec3& values, float resetValue = 0.0f, float columnWidth = 100.0f)
	{
		ImGuiIO& io = ImGui::GetIO();
		auto& boldFont = io.Fonts->Fonts[0];

		ImGui::PushID(label.c_str());

		ImGui::Columns(2);
		ImGui::SetColumnWidth(0, columnWidth);
		ImGui::Text(label.c_str());
		ImGui::NextColumn();

		ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0, 0 });

		float lineHeight = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;
		ImVec2 buttonSize = ImVec2{ lineHeight + 3.0f, lineHeight };

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.2f, 0.2f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
		ImGui::PushFont(boldFont);
		if (ImGui::Button("X", buttonSize))
			values.x = resetValue;
		ImGui::PopFont();

		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::DragFloat("##X", &values.x, 0.1f, 0.0f, 0.0f, "%.2f");
		ImGui::PopItemWidth();
		ImGui::SameLine();

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.3f, 0.8f, 0.3f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
		ImGui::PushFont(boldFont);
		if (ImGui::Button("Y", buttonSize))
			values.y = resetValue;
		ImGui::PopFont();

		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::DragFloat("##Y", &values.y, 0.1f, 0.0f, 0.0f, "%.2f");
		ImGui::PopItemWidth();
		ImGui::SameLine();

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.2f, 0.35f, 0.9f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
		ImGui::PushFont(boldFont);
		if (ImGui::Button("Z", buttonSize))
			values.z = resetValue;
		ImGui::PopFont();

		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::DragFloat("##Z", &values.z, 0.1f, 0.0f, 0.0f, "%.2f");
		ImGui::PopItemWidth();

		ImGui::PopStyleVar();

		ImGui::Columns(1);

		ImGui::PopID();
	}

	// Draws one editable control for a ScriptFieldInstance by its type. Returns true if edited.
	static bool DrawScriptField(const std::string& label, ScriptFieldInstance& field)
	{
		switch (field.Field.Type)
		{
			case ScriptFieldType::Float:
			{
				float v = field.GetValue<float>();
				if (ImGui::DragFloat(label.c_str(), &v, 0.1f)) { field.SetValue(v); return true; }
				break;
			}
			case ScriptFieldType::Int32:
			{
				int v = field.GetValue<int32_t>();
				if (ImGui::DragInt(label.c_str(), &v)) { field.SetValue<int32_t>(v); return true; }
				break;
			}
			case ScriptFieldType::Bool:
			{
				bool v = field.GetValue<bool>();
				if (ImGui::Checkbox(label.c_str(), &v)) { field.SetValue(v); return true; }
				break;
			}
			case ScriptFieldType::Vector2:
			{
				glm::vec2 v = field.GetValue<glm::vec2>();
				if (ImGui::DragFloat2(label.c_str(), glm::value_ptr(v), 0.1f)) { field.SetValue(v); return true; }
				break;
			}
			case ScriptFieldType::Vector3:
			{
				glm::vec3 v = field.GetValue<glm::vec3>();
				if (ImGui::DragFloat3(label.c_str(), glm::value_ptr(v), 0.1f)) { field.SetValue(v); return true; }
				break;
			}
			case ScriptFieldType::Vector4:
			{
				glm::vec4 v = field.GetValue<glm::vec4>();
				if (ImGui::ColorEdit4(label.c_str(), glm::value_ptr(v))) { field.SetValue(v); return true; }
				break;
			}
			default:
				ImGui::TextDisabled("%s (unsupported type)", label.c_str());
				break;
		}
		return false;
	}

	template<typename T, typename UIFunction>
	static void DrawComponent(const std::string& label, Entity entity, UIFunction uiFunction)
	{
		const ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth;

		if (entity.HasComponent<T>())
		{
			auto& component = entity.GetComponent<T>();
			ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
			float lineHeight = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;
			ImGui::Separator();
			bool open = ImGui::TreeNodeEx((void*)typeid(T).hash_code(), treeNodeFlags, label.c_str());
			ImGui::PopStyleVar();
			ImGui::SameLine(contentRegionAvailable.x - lineHeight * 0.5f);
			if (ImGui::Button("+", ImVec2{ lineHeight, lineHeight }))
				ImGui::OpenPopup("ComponentSettings");

			bool componentMarkedForRemoval = false;
			if (ImGui::BeginPopup("ComponentSettings"))
			{
				if (ImGui::MenuItem("Remove Component"))
					componentMarkedForRemoval = true;

				ImGui::EndPopup();
			}

			if (open)
			{
				uiFunction(component);
				ImGui::TreePop();
			}

			if (componentMarkedForRemoval)
				entity.RemoveComponent<T>();
		}
	}

	void SceneHierarchyPanel::DrawComponents(Entity entity)
	{
		if (entity.HasComponent<TagComponent>())
		{
			auto& tag = entity.GetComponent<TagComponent>().Tag;
			char buffer[256];
			memset(buffer, 0, sizeof(buffer));
			strcpy_s(buffer, sizeof(buffer), tag.c_str());

			if (ImGui::InputText("##Tag", buffer, sizeof(buffer)))
				tag = std::string(buffer);
		}

		ImGui::SameLine();
		ImGui::PushItemWidth(-1);

		if (ImGui::Button("Add Component"))
			ImGui::OpenPopup("AddComponent");

		if (ImGui::BeginPopup("AddComponent"))
		{
			DisplayAddComponentEntry<CameraComponent>("Camera");
			DisplayAddComponentEntry<ScriptComponent>("Script");
			DisplayAddComponentEntry<SpriteRendererComponent>("Sprite Renderer");
			DisplayAddComponentEntry<CircleRendererComponent>("Circle Renderer");
			DisplayAddComponentEntry<Rigidbody2DComponent>("Rigidbody 2D");
			DisplayAddComponentEntry<BoxCollider2DComponent>("Box Collider 2D");
			DisplayAddComponentEntry<CircleCollider2DComponent>("Circle Collider 2D");
			DisplayAddComponentEntry<TextComponent>("Text Renderer");

			// User-defined (C#) components
			for (const auto& [typeName, scriptClass] : ScriptEngine::GetComponentClasses())
			{
				std::string label(typeName.begin(), typeName.end());
				bool alreadyHas = false;
				if (m_SelectionContext.HasComponent<ManagedComponentStorage>())
				{
					auto& storage = m_SelectionContext.GetComponent<ManagedComponentStorage>();
					alreadyHas = storage.Components.find(typeName) != storage.Components.end();
				}
				if (!alreadyHas && ImGui::MenuItem(label.c_str()))
				{
					ScriptEngine::AddManagedComponent(m_SelectionContext, typeName);
					ImGui::CloseCurrentPopup();
				}
			}

			ImGui::EndPopup();
		}

		ImGui::PopItemWidth();

		DrawComponent<TransformComponent>("Transform", entity, [](auto& component)
		{
			DrawVec3Control("Position", component.Translation);

			glm::vec3 rotation = glm::degrees(component.Rotation);
			DrawVec3Control("Rotation", rotation);
			component.Rotation = glm::radians(rotation);

			DrawVec3Control("Scale", component.Scale, 1.0f);
		});

		DrawComponent<CameraComponent>("Camera", entity, [](auto& component)
		{
			auto& camera = component.Camera;

			const char* projectionTypeStrings[] = { "Perspective", "Orthographic" };
			const char* currentProjectionTypeString = projectionTypeStrings[(int)camera.GetProjectionType()];

			ImGui::Checkbox("Primary", &component.Primary);

			if (ImGui::BeginCombo("Projection", currentProjectionTypeString))
			{
				for (int i = 0; i < 2; ++i)
				{
					bool isSelected = currentProjectionTypeString == projectionTypeStrings[i];
					if (ImGui::Selectable(projectionTypeStrings[i], isSelected))
					{
						currentProjectionTypeString = projectionTypeStrings[i];
						camera.SetProjectionType((SceneCamera::ProjectionType)i);
					}

					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}

				ImGui::EndCombo();
			}

			if (camera.GetProjectionType() == SceneCamera::ProjectionType::Perspective)
			{
				float verticalFOV = glm::degrees(camera.GetPerspectiveVerticalFOV());
				if (ImGui::DragFloat("Vertical FOV", &verticalFOV, 0.1f))
					camera.SetPerspectiveVerticalFOV(verticalFOV);

				float perspectiveNear = camera.GetPerspectiveNearClip();
				if (ImGui::DragFloat("Near Clip", &perspectiveNear, 0.1f))
					camera.SetPerspectiveNearClip(perspectiveNear);

				float perspectiveFar = camera.GetPerspectiveFarClip();
				if (ImGui::DragFloat("Far Clip", &perspectiveFar, 0.1f))
					camera.SetPerspectiveFarClip(perspectiveFar);
			}

			if (camera.GetProjectionType() == SceneCamera::ProjectionType::Orthographic)
			{
				float orthoSize = camera.GetOrthographicSize();
				if (ImGui::DragFloat("Size", &orthoSize, 0.1f))
					camera.SetOrthographicSize(orthoSize);

				float orthoNear = camera.GetOrthographicNearClip();
				if (ImGui::DragFloat("Near Clip", &orthoNear, 0.1f))
					camera.SetOrthographicNearClip(orthoNear);

				float orthoFar = camera.GetOrthographicFarClip();
				if (ImGui::DragFloat("Far Clip", &orthoFar, 0.1f))
					camera.SetOrthographicFarClip(orthoFar);

				ImGui::Checkbox("Fixed Aspect Ratio", &component.FixedAspectRatio);
			}
		});

		DrawComponent<ScriptComponent>("Script", entity, [entity, scene = m_Context](auto& component) mutable
		{
			bool scriptClassExists = ScriptEngine::EntityClassExists(component.ModuleName);

			static char moduleBuffer[256];
			wcstombs_s(nullptr, moduleBuffer, sizeof(moduleBuffer), component.ModuleName.c_str(), _TRUNCATE);

			Timefall::UI::ScopedStyleColor textColor(ImGuiCol_Text, ImVec4{ 0.9f, 0.2f, 0.3f, 1.0f }, !scriptClassExists);

			if (ImGui::InputText("Class", moduleBuffer, sizeof(moduleBuffer)))
			{
				size_t len = std::strlen(moduleBuffer);
				component.ModuleName.resize(len);
				mbstowcs_s(nullptr, component.ModuleName.data(), len + 1, moduleBuffer, _TRUNCATE);
				return;
			}

			// Fields
			bool sceneRunning = scene->IsRunning();
			if (sceneRunning)
			{
				Ref<ScriptInstance> scriptInstance = ScriptEngine::GetEntityScriptInstance(entity.GetUUID());
				if (scriptInstance)
				{
					const auto& fields = scriptInstance->GetScriptClass()->GetFields();
					for (const auto& [fieldName, field] : fields)
					{
						if (field.Type == ScriptFieldType::Float)
						{
							float data = scriptInstance->GetFieldValue<float>(fieldName);
							char nameBuffer[256];
							wcstombs_s(nullptr, nameBuffer, sizeof(nameBuffer), fieldName.c_str(), _TRUNCATE);
							if (ImGui::DragFloat(nameBuffer, &data))
								scriptInstance->SetFieldValue(fieldName, data);
						}
					}
				}
			}
			else
			{
				if (scriptClassExists)
				{
					Ref<ScriptClass> entityClass = ScriptEngine::GetEntityScriptClass(component.ModuleName);
					const auto& fields = entityClass->GetFields();

					auto& entityFields = ScriptEngine::GetEntityScriptFields(entity);
					for (const auto& [name, field] : fields)
					{
						// Field has been set in editor
						if (entityFields.find(name) != entityFields.end())
						{
							ScriptFieldInstance& scriptField = entityFields.at(name);

							// Display control to set it maybe
							if (field.Type == ScriptFieldType::Float)
							{
								float data = scriptField.GetValue<float>();
								char nameBuffer[256];
								wcstombs_s(nullptr, nameBuffer, sizeof(nameBuffer), name.c_str(), _TRUNCATE);
								if (ImGui::DragFloat(nameBuffer, &data))
									scriptField.SetValue(data);
							}
						}
						else
						{
							// Display control to set it maybe
							if (field.Type == ScriptFieldType::Float)
							{
								float data = 0.0f;
								char nameBuffer[256];
								wcstombs_s(nullptr, nameBuffer, sizeof(nameBuffer), name.c_str(), _TRUNCATE);
								if (ImGui::DragFloat(nameBuffer, &data))
								{
									ScriptFieldInstance& fieldInstance = entityFields[name];
									fieldInstance.Field = field;
									fieldInstance.SetValue(data);
								}
							}
						}
					}
				}
			}
		});

		DrawComponent<SpriteRendererComponent>("Sprite Renderer", entity, [](auto& component)
		{
			ImGui::ColorEdit4("Color", glm::value_ptr(component.Color));

			std::string label = "None";
			bool isTextureValid = false;
			if (component.Texture != (AssetHandle)0)
			{
				if (AssetManager::IsAssetHandleValid(component.Texture) && AssetManager::GetAssetType(component.Texture) == AssetType::Texture2D)
				{
					const AssetMetadata& metadata = Project::GetActive()->GetEditorAssetManager()->GetMetadata(component.Texture);
					label = metadata.FilePath.filename().string();
					isTextureValid = true;
				}
				else
				{
					label = "Invalid Texture";
				}
			}

			ImVec2 buttonLabelSize = ImGui::CalcTextSize(label.c_str());
			buttonLabelSize.x += 20.0f; // Add some padding for the button
			float buttonWidth = glm::max<float>(buttonLabelSize.x, 100.0f);

			ImGui::Button(label.c_str(), ImVec2(buttonWidth, 0.0f));
			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
				{
					AssetHandle handle = *(AssetHandle*)payload->Data;
					if (AssetManager::GetAssetType(handle) == AssetType::Texture2D)
					{
						component.Texture = handle;
					}
					else
					{
						TF_CORE_WARN("Dropped asset is not a texture!");
					}
				}

				ImGui::EndDragDropTarget();
			}

			if (isTextureValid)
			{
				ImGui::SameLine();
				ImVec2 xLabelSize = ImGui::CalcTextSize("X");
				float buttonSize = xLabelSize.y + ImGui::GetStyle().FramePadding.y * 2.0f;
				if (ImGui::Button("X", ImVec2{ buttonSize, buttonSize }))
					component.Texture = 0;
			}
			ImGui::SameLine();
			ImGui::Text("Texture");

			ImGui::DragFloat("Tiling Factor", &component.TilingFactor, 0.1f, 0.0f, 100.0f);
		});
		
		DrawComponent<CircleRendererComponent>("Circle Renderer", entity, [](auto& component)
		{
			ImGui::ColorEdit4("Color", glm::value_ptr(component.Color));
			ImGui::DragFloat("Thickness", &component.Thickness, 0.025f, 0.0f, 1.0f);
			ImGui::DragFloat("Fade", &component.Fade, 0.00025f, 0.0f, 1.0f);
		});

		DrawComponent<Rigidbody2DComponent>("Rigidbody 2D", entity, [](auto& component)
		{
			const char* bodyTypeStrings[] = { "Static", "Kinematic", "Dynamic" };
			const char* currentBodyTypeString = bodyTypeStrings[(int)component.Type];
			if (ImGui::BeginCombo("Body Type", currentBodyTypeString))
			{
				for (int i = 0; i < 3; i++)
				{
					bool isSelected = currentBodyTypeString == bodyTypeStrings[i];
					if (ImGui::Selectable(bodyTypeStrings[i], isSelected))
					{
						currentBodyTypeString = bodyTypeStrings[i];
						component.Type = (Rigidbody2DComponent::BodyType)i;
					}

					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}

				ImGui::EndCombo();
			}

			ImGui::Checkbox("Fixed Rotation", &component.FixedRotation);
		});

		DrawComponent<BoxCollider2DComponent>("Box Collider 2D", entity, [this](auto& component)
		{
			ImGui::DragFloat2("Offset", glm::value_ptr(component.Offset));
			ImGui::DragFloat2("Size", glm::value_ptr(component.Size));
			ImGui::DragFloat("Density", &component.Density, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Friction", &component.Friction, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Restitution", &component.Restitution, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Restitution Threshold (Global)", &m_Context->m_RestitutionThreshold, 0.01f, 0.0f);
		});
		
		DrawComponent<CircleCollider2DComponent>("Circle Collider 2D", entity, [this](auto& component)
		{
			ImGui::DragFloat2("Offset", glm::value_ptr(component.Offset));
			ImGui::DragFloat("Radius", &component.Radius, 0.01f, 0.0f);
			ImGui::DragFloat("Density", &component.Density, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Friction", &component.Friction, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Restitution", &component.Restitution, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Restitution Threshold (Global)", &m_Context->m_RestitutionThreshold, 0.01f, 0.0f);
		});
		
		DrawComponent<TextComponent>("Text Renderer", entity, [this](auto& component)
		{
			ImGui::InputTextMultiline("Text", &component.Text);
			ImGui::ColorEdit4("Color", glm::value_ptr(component.Color));
			ImGui::DragFloat("Kerning", &component.Kerning, 0.025f);
			ImGui::DragFloat("Line Spacing", &component.LineSpacing, 0.025f);
		});

		// User-defined (managed) components — native storage is the source of truth, so we read/write
		// its ScriptFieldInstance buffers directly (same path in edit and play modes).
		if (entity.HasComponent<ManagedComponentStorage>())
		{
			auto& storage = entity.GetComponent<ManagedComponentStorage>();
			std::vector<std::wstring> toRemove;

			for (auto& [typeName, data] : storage.Components)
			{
				std::string label(typeName.begin(), typeName.end());
				ImGui::PushID(label.c_str());

				// Mirror DrawComponent: measure region BEFORE the node and set AllowItemOverlap so the
				// full-width header doesn't swallow the "+" button's click.
				ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();
				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
				float lineHeight = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;
				ImGui::Separator();
				bool open = ImGui::TreeNodeEx(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth);
				ImGui::PopStyleVar();
				ImGui::SameLine(contentRegionAvailable.x - lineHeight * 0.5f);
				if (ImGui::Button("+", ImVec2{ lineHeight, lineHeight }))
					ImGui::OpenPopup("ManagedComponentSettings");

				if (ImGui::BeginPopup("ManagedComponentSettings"))
				{
					if (ImGui::MenuItem("Remove Component"))
						toRemove.push_back(typeName);
					ImGui::EndPopup();
				}

				if (open)
				{
					if (Ref<ScriptClass> schema = ScriptEngine::GetComponentClass(typeName))
					{
						for (const auto& [fieldName, field] : schema->GetFields())
						{
							ScriptFieldInstance& inst = data.Fields[fieldName];
							inst.Field = field; // schema is authoritative for name+type (handles new/changed fields; value buffer preserved)
							std::string fieldLabel(fieldName.begin(), fieldName.end());
							DrawScriptField(fieldLabel, inst);
						}
					}
					else
					{
						for (auto& [fieldName, inst] : data.Fields)
						{
							std::string fieldLabel(fieldName.begin(), fieldName.end());
							DrawScriptField(fieldLabel, inst);
						}
					}
					ImGui::TreePop();
				}

				ImGui::PopID();
			}

			for (const std::wstring& typeName : toRemove)
				storage.Components.erase(typeName);
		}
	}

	template<typename T>
	void SceneHierarchyPanel::DisplayAddComponentEntry(const std::string& entryName)
	{
		if (!m_SelectionContext.HasComponent<T>())
		{
			if (ImGui::MenuItem(entryName.c_str()))
			{
				m_SelectionContext.AddComponent<T>();
				ImGui::CloseCurrentPopup();
			}
		}
	}
}