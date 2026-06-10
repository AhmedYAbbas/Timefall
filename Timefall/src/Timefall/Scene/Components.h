#pragma once

#include "Timefall/Core/UUID.h"
#include "Timefall/Scene/SceneCamera.h"
#include "Timefall/Renderer/Texture.h"
#include "Timefall/Renderer/Font.h"
#include "Timefall/Renderer/Mesh.h"
#include "Timefall/Math/Math.h"
#include "Timefall/Scripting/ScriptEngine.h" // ScriptFieldInstance / ScriptFieldMap

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

#include <vector>

namespace Timefall
{
	struct TF_API IDComponent
	{
		UUID ID;

		IDComponent() = default;
		IDComponent(const IDComponent&) = default;
		IDComponent(const UUID& uuid)
			: ID(uuid)
		{
		}
	};

	struct TF_API TagComponent
	{
		std::string Tag;

		TagComponent() = default;
		TagComponent(const TagComponent&) = default;
		TagComponent(const std::string& tag)
			: Tag(tag)
		{
		}
	};

	struct TF_API TransformComponent
	{
		glm::vec3 Translation { 0.0f };
		glm::vec3 Rotation { 0.0f };
		glm::vec3 Scale { 1.0f };

		TransformComponent() = default;
		TransformComponent(const TransformComponent&) = default;
		TransformComponent(const glm::vec3& position)
			: Translation(position)
		{
		}

		// Composes this component's LOCAL transform (T * R * S). Rotation is Euler radians.
		// World transform is resolved by Entity::GetWorldTransform() (it must walk the parent chain).
		glm::mat4 GetLocalTransform() const
		{
			return glm::translate(glm::mat4(1.0f), Translation)
				* glm::toMat4(glm::quat(Rotation))
				* glm::scale(glm::mat4(1.0f), Scale);
		}

		// Overwrites local T/R/S by decomposing a local matrix. Lossless under uniform scale;
		// drops shear/perspective it cannot represent. Used by reparent / SetWorldTransform.
		void SetLocalTransform(const glm::mat4& transform)
		{
			Math::DecomposeTransform(transform, Translation, Rotation, Scale);
		}
	};

	// Scene-graph link. Added lazily (only on parented entities) — absence means a root entity.
	// Parent == 0 is the reserved "no parent" sentinel. Stores UUIDs (not entt handles) so it
	// survives save/load, Scene::Copy, and duplication.
	struct TF_API RelationshipComponent
	{
		UUID Parent = 0;
		std::vector<UUID> Children;

		RelationshipComponent() = default;
		RelationshipComponent(const RelationshipComponent&) = default;
	};

	struct TF_API SpriteRendererComponent
	{
		glm::vec4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };
		AssetHandle Texture = 0;
		float TilingFactor = 1.0f;

		SpriteRendererComponent() = default;
		SpriteRendererComponent(const SpriteRendererComponent&) = default;
		SpriteRendererComponent(const glm::vec4& color)
			: Color(color)
		{
		}
	};
	
	struct TF_API CircleRendererComponent
	{
		glm::vec4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };
		float Thickness = 1.0f;
		float Fade = 0.005f;

		CircleRendererComponent() = default;
		CircleRendererComponent(const CircleRendererComponent&) = default;
		CircleRendererComponent(const glm::vec4& color)
			: Color(color)
		{
		}
	};

	// 9.1 interim: selects a built-in primitive. Becomes a mesh AssetHandle in 9.4.
	struct TF_API MeshComponent
	{
		PrimitiveType Type = PrimitiveType::Cube;

		MeshComponent() = default;
		MeshComponent(const MeshComponent&) = default;
	};

	struct TF_API LightComponent
	{
		enum class LightType : uint8_t { Directional = 0, Point = 1, Spot = 2 };

		LightType Type = LightType::Directional;
		glm::vec3 Color{ 1.0f, 1.0f, 1.0f };
		float Intensity = 1.0f;

		// Point / Spot: distance at which the light fades to zero.
		float Range = 10.0f;

		// Spot only (degrees). Inner = full brightness cone, Outer = falloff edge.
		float InnerCutoff = 12.5f;
		float OuterCutoff = 17.5f;

		LightComponent() = default;
		LightComponent(const LightComponent&) = default;
	};

	namespace Utils
	{
		static const char* LightTypeToString(LightComponent::LightType type)
		{
			switch (type)
			{
				case LightComponent::LightType::Directional: return "Directional";
				case LightComponent::LightType::Point:       return "Point";
				case LightComponent::LightType::Spot:        return "Spot";
			}
			TF_CORE_ASSERT(false, "Unknown LightType");
			return "Directional";
		}

		static LightComponent::LightType LightTypeFromString(const std::string& type)
		{
			if (type == "Directional") return LightComponent::LightType::Directional;
			if (type == "Point")       return LightComponent::LightType::Point;
			if (type == "Spot")        return LightComponent::LightType::Spot;

			TF_CORE_ASSERT(false, "Unknown LightType string");
			return LightComponent::LightType::Directional;
		}
	}

	struct TF_API CameraComponent
	{
		SceneCamera Camera;
		bool Primary = true;
		bool FixedAspectRatio = false;

		CameraComponent() = default;
		CameraComponent(const CameraComponent&) = default;
	};

	struct TF_API ScriptComponent
	{
		std::wstring ModuleName;

		ScriptComponent() = default;
		ScriptComponent(const ScriptComponent&) = default;
		ScriptComponent(const std::wstring& moduleName)
			: ModuleName(moduleName)
		{
		}
	};

	// Physics
	struct TF_API Rigidbody2DComponent
	{
		enum class BodyType
		{
			Static = 0,
			Kinematic = 1,
			Dynamic = 2
		};

		BodyType Type = BodyType::Static;
		bool FixedRotation = false;

		Rigidbody2DComponent() = default;
		Rigidbody2DComponent(const Rigidbody2DComponent&) = default;
	};

	struct TF_API BoxCollider2DComponent
	{
		glm::vec2 Offset{ 0.0f };
		glm::vec2 Size{ 0.5f };

		// TODO: move to physics materials
		float Density = 1.0f;
		float Friction = 0.5f;
		float Restitution = 0.0f;

		BoxCollider2DComponent() = default;
		BoxCollider2DComponent(const BoxCollider2DComponent&) = default;
	};

	struct TF_API CircleCollider2DComponent
	{
		glm::vec2 Offset{ 0.0f };
		float Radius = 0.5f;

		// TODO: move to physics materials
		float Density = 1.0f;
		float Friction = 0.5f;
		float Restitution = 0.0f;

		CircleCollider2DComponent() = default;
		CircleCollider2DComponent(const CircleCollider2DComponent&) = default;
	};
	
	struct TF_API TextComponent
	{
		std::string Text;
		Ref<Font> FontAsset = Font::GetDefault();
		glm::vec4 Color{ 1.0f };
		float Kerning = 0.0f;
		float LineSpacing = 0.0f;

		TextComponent() = default;
		TextComponent(const TextComponent&) = default;
	};


	class ScriptableEntity;
	struct TF_API NativeScriptComponent
	{
		ScriptableEntity* Instance = nullptr;

		ScriptableEntity* (*InstantiateScript)();
		void (*DestroyScript)(NativeScriptComponent*);

		template<typename T>
		void Bind()
		{
			InstantiateScript = []() { return static_cast<ScriptableEntity*>(new T()); };
			DestroyScript = [](NativeScriptComponent* nsc) { delete nsc->Instance; nsc->Instance = nullptr; };
		}

		operator bool() const { return Instance; }
	};

	// One user-defined (C#-declared) component's data: a name->typed-value-buffer map.
	// Reuses the ScriptField stack verbatim (ScriptFieldMap = unordered_map<wstring, ScriptFieldInstance>).
	struct TF_API ManagedComponentData
	{
		ScriptFieldMap Fields;

		ManagedComponentData() = default;
		ManagedComponentData(const ManagedComponentData&) = default;
	};

	// Type-erased holder for every user-defined component on an entity, keyed by the C# type
	// full-name (e.g. L"Sandbox.BlockTag"). Pure data: no UUIDs, no GCHandle, so it copies and
	// hot-reloads trivially. Source of truth for user-component values in every context.
	struct TF_API ManagedComponentStorage
	{
		std::unordered_map<std::wstring, ManagedComponentData> Components;

		ManagedComponentStorage() = default;
		ManagedComponentStorage(const ManagedComponentStorage&) = default;
	};

	template<typename... T>
	struct TF_API ComponentGroup
	{
	};

	using AllComponents = ComponentGroup<
		TransformComponent,
		RelationshipComponent,
		SpriteRendererComponent,
		CircleRendererComponent,
		MeshComponent,
		LightComponent,
		CameraComponent,
		ScriptComponent,
		Rigidbody2DComponent,
		BoxCollider2DComponent,
		CircleCollider2DComponent,
		TextComponent,
		ManagedComponentStorage,
		NativeScriptComponent>;
}