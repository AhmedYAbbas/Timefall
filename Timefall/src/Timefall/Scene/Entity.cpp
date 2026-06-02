#include "tfpch.h"
#include "Entity.h"

#include "Timefall/Scene/Scene.h"
#include "Timefall/Math/Math.h"

namespace Timefall
{
	Entity::Entity(entt::entity entity, Scene* scene)
		: m_EntityHandle(entity), m_Scene(scene)
	{
	}

	glm::mat4 Entity::GetParentWorldTransform()
	{
		// A RelationshipComponent is added lazily, so its absence simply means "root entity".
		// Root (or Parent == 0) resolves to identity — never assert on a missing relationship.
		if (HasComponent<RelationshipComponent>())
		{
			UUID parentID = GetComponent<RelationshipComponent>().Parent;
			if (parentID != 0)
			{
				Entity parent = m_Scene->GetEntityByUUID(parentID);
				if (parent)
					return parent.GetWorldTransform();
			}
		}

		return glm::mat4(1.0f);
	}

	glm::mat4 Entity::GetWorldTransform()
	{
		TF_CORE_ASSERT(HasComponent<TransformComponent>(), "Entity does not have a TransformComponent");

		const auto& tc = GetComponent<TransformComponent>();
		return GetParentWorldTransform() * tc.GetLocalTransform();
	}

	void Entity::SetWorldTransform(const glm::mat4& worldTransform)
	{
		TF_CORE_ASSERT(HasComponent<TransformComponent>(), "Entity does not have a TransformComponent");

		// Solve for the local transform that yields the requested world transform under our parent,
		// then store it (SetLocalTransform decomposes into T/R/S, rotation in Euler radians).
		glm::mat4 localTransform = glm::inverse(GetParentWorldTransform()) * worldTransform;
		GetComponent<TransformComponent>().SetLocalTransform(localTransform);
	}

	glm::vec3 Entity::GetWorldTranslation()
	{
		// Translation is the 4th column of the world matrix — no decomposition needed.
		return glm::vec3(GetWorldTransform()[3]);
	}

	void Entity::SetWorldTranslation(const glm::vec3& translation)
	{
		TF_CORE_ASSERT(HasComponent<TransformComponent>(), "Entity does not have a TransformComponent");

		// Pure point solve: only local translation changes; local rotation/scale are preserved.
		glm::vec3 localTranslation = glm::vec3(glm::inverse(GetParentWorldTransform()) * glm::vec4(translation, 1.0f));
		GetComponent<TransformComponent>().Translation = localTranslation;
	}

	glm::vec3 Entity::GetWorldRotation()
	{
		glm::vec3 translation, rotation, scale;
		Math::DecomposeTransform(GetWorldTransform(), translation, rotation, scale);
		return rotation; // Euler radians
	}

	void Entity::SetWorldRotation(const glm::vec3& rotation)
	{
		TF_CORE_ASSERT(HasComponent<TransformComponent>(), "Entity does not have a TransformComponent");

		// We want: parentWorldRotation * localRotation == targetWorldRotation.
		// World position is auto-preserved (rotation doesn't touch the translation column) and local
		// scale is untouched (we only write Rotation).
		glm::vec3 parentTranslation, parentRotation, parentScale;
		Math::DecomposeTransform(GetParentWorldTransform(), parentTranslation, parentRotation, parentScale);

		glm::quat parentWorldRot = glm::quat(parentRotation);      // Euler radians -> quat
		glm::quat targetWorldRot = glm::quat(rotation);
		glm::quat localRot = glm::inverse(parentWorldRot) * targetWorldRot;

		// Run the resulting local rotation back through the same decomposition convention used
		// everywhere else, so the stored Euler matches what the inspector / gizmo expect.
		glm::vec3 dTranslation, localEuler, dScale;
		Math::DecomposeTransform(glm::toMat4(localRot), dTranslation, localEuler, dScale);
		GetComponent<TransformComponent>().Rotation = localEuler;
	}

	glm::vec3 Entity::GetWorldScale()
	{
		// Read-only "lossy scale": exact for uniform scale / axis-aligned rotation, approximate
		// (drops shear it can't represent) otherwise. By design there is no SetWorldScale.
		glm::vec3 translation, rotation, scale;
		Math::DecomposeTransform(GetWorldTransform(), translation, rotation, scale);
		return scale;
	}
}
