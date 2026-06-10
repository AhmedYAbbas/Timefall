#include "tfpch.h"

#include "Timefall/Scene/SceneSerializer.h"
#include "Timefall/Scene/Entity.h"
#include "Timefall/Scene/Components.h"
#include "Timefall/Scripting/ScriptEngine.h"
#include "Timefall/Core/UUID.h"
#include "Timefall/Project/Project.h"
#include "Timefall/Physics/Physics2D.h"

#include <yaml-cpp/yaml.h>

namespace YAML
{
	template<>
	struct convert<glm::vec2>
	{
		static Node encode(const glm::vec2& rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.SetStyle(EmitterStyle::Flow);
			return node;
		}

		static bool decode(const Node& node, glm::vec2& rhs)
		{
			if (!node.IsSequence() || node.size() != 2)
				return false;

			rhs.x = node[0].as<float>();
			rhs.y = node[1].as<float>();
			return true;
		}
	};

	template<>
	struct convert<glm::vec3>
	{
		static Node encode(const glm::vec3& v)
		{
			Node node;
			node.push_back(v.x);
			node.push_back(v.y);
			node.push_back(v.z);
			return node;
		}

		static bool decode(const Node& node, glm::vec3& v)
		{
			if (!node.IsSequence() || node.size() != 3)
				return false;

			v.x = node[0].as<float>();
			v.y = node[1].as<float>();
			v.z = node[2].as<float>();
			return true;
		}
	};

	template<>
	struct convert<glm::vec4>
	{
		static Node encode(const glm::vec4& v)
		{
			Node node;
			node.push_back(v.x);
			node.push_back(v.y);
			node.push_back(v.z);
			node.push_back(v.w);
			return node;
		}

		static bool decode(const Node& node, glm::vec4& v)
		{
			if (!node.IsSequence() || node.size() != 4)
				return false;
			v.x = node[0].as<float>();
			v.y = node[1].as<float>();
			v.z = node[2].as<float>();
			v.w = node[3].as<float>();
			return true;
		}
	};

	template<>
	struct convert<std::wstring>
	{
		static Node encode(const std::wstring& wstr)
		{
			Node node;
			std::string str(wstr.begin(), wstr.end());
			node = str;
			return node;
		}

		static bool decode(const Node& node, std::wstring& wstr)
		{
			if (!node.IsScalar())
				return false;

			std::string str = node.as<std::string>();
			wstr = std::wstring(str.begin(), str.end());
			return true;
		}
	};

	template<>
	struct convert < Timefall::UUID >
	{
		static Node encode(const Timefall::UUID& uuid)
		{
			Node node;
			node.push_back((uint64_t)uuid);
			return node;
		}

		static bool decode(const Node& node, Timefall::UUID& uuid)
		{
			uuid = node.as<uint64_t>();
			return true;
		}
	};
}

namespace Timefall
{
#define WRITE_SCRIPT_FIELD(FieldType, Type)           \
			case ScriptFieldType::FieldType:          \
				out << scriptField.GetValue<Type>();  \
				break

#define READ_SCRIPT_FIELD(FieldType, Type)             \
	case ScriptFieldType::FieldType:                   \
	{                                                  \
		Type data = scriptField["Data"].as<Type>();    \
		fieldInstance.SetValue(data);                  \
		break;                                         \
	}

	YAML::Emitter& operator<<(YAML::Emitter& out, const glm::vec2& v)
	{
		out << YAML::Flow;
		out << YAML::BeginSeq << v.x << v.y << YAML::EndSeq;
		return out;
	}

	YAML::Emitter& operator<<(YAML::Emitter& out, const glm::vec3& v)
	{
		out << YAML::Flow << YAML::BeginSeq << v.x << v.y << v.z << YAML::EndSeq;
		return out;
	}

	YAML::Emitter& operator<<(YAML::Emitter& out, const glm::vec4& v)
	{
		out << YAML::Flow << YAML::BeginSeq << v.x << v.y << v.z << v.w << YAML::EndSeq;
		return out;
	}

	YAML::Emitter& operator<<(YAML::Emitter& out, const std::wstring& wstr)
	{
		std::string str(wstr.begin(), wstr.end());
		out << str;
		return out;
	}

	SceneSerializer::SceneSerializer(const Ref<Scene>& scene)
		: m_Scene(scene)
	{
	}

	static void SerializeEntity(YAML::Emitter& out, Entity entity)
	{
		out << YAML::BeginMap; // Entity
		out << YAML::Key << "Entity" << YAML::Value << entity.GetUUID();

		if (entity.HasComponent<TagComponent>())
		{
			out << YAML::Key << "TagComponent";
			out << YAML::BeginMap; // TagComponent;

			auto& tag = entity.GetComponent<TagComponent>().Tag;
			out << YAML::Key << "Tag" << YAML::Value << tag;

			out << YAML::EndMap; // TagComponent
		}

		if (entity.HasComponent<TransformComponent>())
		{
			out << YAML::Key << "TransformComponent";
			out << YAML::BeginMap; // TransformComponent

			auto& tc = entity.GetComponent<TransformComponent>();
			out << YAML::Key << "Position" << YAML::Value << tc.Translation;
			out << YAML::Key << "Rotation" << YAML::Value << tc.Rotation;
			out << YAML::Key << "Scale" << YAML::Value << tc.Scale;

			out << YAML::EndMap; // TransformComponent
		}

		if (entity.HasComponent<RelationshipComponent>())
		{
			out << YAML::Key << "RelationshipComponent";
			out << YAML::BeginMap; // RelationshipComponent

			auto& rc = entity.GetComponent<RelationshipComponent>();
			out << YAML::Key << "Parent" << YAML::Value << (uint64_t)rc.Parent;

			out << YAML::Key << "Children" << YAML::Value << YAML::BeginSeq;
			for (UUID child : rc.Children)
				out << (uint64_t)child;
			out << YAML::EndSeq;

			out << YAML::EndMap; // RelationshipComponent
		}

		if (entity.HasComponent<CameraComponent>())
		{
			out << YAML::Key << "CameraComponent";
			out << YAML::BeginMap; // CameraComponent

			auto& cameraComponent = entity.GetComponent<CameraComponent>();
			auto& camera = cameraComponent.Camera;

			out << YAML::Key << "Camera" << YAML::Value;
			out << YAML::BeginMap; // Camera
			out << YAML::Key << "ProjectionType" << YAML::Value << (int)camera.GetProjectionType();
			out << YAML::Key << "PerspectiveFOV" << YAML::Value << glm::degrees(camera.GetPerspectiveVerticalFOV());
			out << YAML::Key << "PerspectiveNear" << YAML::Value << camera.GetPerspectiveNearClip();
			out << YAML::Key << "PerspectiveFar" << YAML::Value << camera.GetPerspectiveFarClip();
			out << YAML::Key << "OrthographicSize" << YAML::Value << camera.GetOrthographicSize();
			out << YAML::Key << "OrthographicNear" << YAML::Value << camera.GetOrthographicNearClip();
			out << YAML::Key << "OrthographicFar" << YAML::Value << camera.GetOrthographicFarClip();
			out << YAML::EndMap; // Camera

			out << YAML::Key << "Primary" << YAML::Value << cameraComponent.Primary;
			out << YAML::Key << "FixedAspectRatio" << YAML::Value << cameraComponent.FixedAspectRatio;

			out << YAML::EndMap; // CameraComponent
		}

		if (entity.HasComponent<ScriptComponent>())
		{
			out << YAML::Key << "ScriptComponent";
			out << YAML::BeginMap; // ScriptComponent

			auto& scriptComponent = entity.GetComponent<ScriptComponent>();
			out << YAML::Key << "ModuleName" << YAML::Value << scriptComponent.ModuleName;

			// ScriptFields
			Ref<ScriptClass> scriptClass = ScriptEngine::GetEntityScriptClass(scriptComponent.ModuleName);
			auto& fields = scriptClass->GetFields();
			if (fields.size() > 0)
			{
				out << YAML::Key << "ScriptFields" << YAML::Value;
				auto& entityFields = ScriptEngine::GetEntityScriptFields(entity);
				out << YAML::BeginSeq;
				for (const auto& [name, field] : fields)
				{
					if (entityFields.find(name) == entityFields.end())
						continue;

					out << YAML::BeginMap; // ScriptField
					out << YAML::Key << "Name" << YAML::Value << name;
					out << YAML::Key << "Type" << YAML::Value << Utils::ScriptFieldTypeToString(field.Type);

					out << YAML::Key << "Data" << YAML::Value;
					ScriptFieldInstance& scriptField = entityFields.at(name);
					switch (field.Type)
					{
						WRITE_SCRIPT_FIELD(Float,   float);
						WRITE_SCRIPT_FIELD(Double,  double);
						WRITE_SCRIPT_FIELD(Bool,    bool);
						WRITE_SCRIPT_FIELD(Char,    char);
						WRITE_SCRIPT_FIELD(SByte,   int8_t);
						WRITE_SCRIPT_FIELD(Int16,   int16_t);
						WRITE_SCRIPT_FIELD(Int32,   int32_t);
						WRITE_SCRIPT_FIELD(Int64,   int64_t);
						WRITE_SCRIPT_FIELD(Byte,    uint8_t);
						WRITE_SCRIPT_FIELD(UInt16,  uint16_t);
						WRITE_SCRIPT_FIELD(UInt32,  uint32_t);
						WRITE_SCRIPT_FIELD(UInt64,  uint64_t);
						WRITE_SCRIPT_FIELD(Vector2, glm::vec2);
						WRITE_SCRIPT_FIELD(Vector3, glm::vec3);
						WRITE_SCRIPT_FIELD(Vector4, glm::vec4);
						WRITE_SCRIPT_FIELD(Entity,  UUID);
					}
					out << YAML::EndMap; // ScriptField
				}
				out << YAML::EndSeq;
			}

			out << YAML::EndMap; // ScriptComponent
		}

		if (entity.HasComponent<ManagedComponentStorage>())
		{
			auto& storage = entity.GetComponent<ManagedComponentStorage>();
			if (!storage.Components.empty())
			{
				out << YAML::Key << "ManagedComponents" << YAML::Value << YAML::BeginSeq;
				for (auto& [typeName, data] : storage.Components)
				{
					out << YAML::BeginMap; // ManagedComponent
					out << YAML::Key << "TypeName" << YAML::Value << typeName;
					out << YAML::Key << "Fields" << YAML::Value << YAML::BeginSeq;
					for (auto& [name, fieldConst] : data.Fields)
					{
						ScriptFieldInstance scriptField = fieldConst; // copy: GetValue<T>() (used by the macro) is non-const
						out << YAML::BeginMap; // Field
						out << YAML::Key << "Name" << YAML::Value << name;
						out << YAML::Key << "Type" << YAML::Value << Utils::ScriptFieldTypeToString(scriptField.Field.Type);
						out << YAML::Key << "Data" << YAML::Value;
						switch (scriptField.Field.Type)
						{
							WRITE_SCRIPT_FIELD(Float,   float);
							WRITE_SCRIPT_FIELD(Double,  double);
							WRITE_SCRIPT_FIELD(Bool,    bool);
							WRITE_SCRIPT_FIELD(Char,    char);
							WRITE_SCRIPT_FIELD(SByte,   int8_t);
							WRITE_SCRIPT_FIELD(Int16,   int16_t);
							WRITE_SCRIPT_FIELD(Int32,   int32_t);
							WRITE_SCRIPT_FIELD(Int64,   int64_t);
							WRITE_SCRIPT_FIELD(Byte,    uint8_t);
							WRITE_SCRIPT_FIELD(UInt16,  uint16_t);
							WRITE_SCRIPT_FIELD(UInt32,  uint32_t);
							WRITE_SCRIPT_FIELD(UInt64,  uint64_t);
							WRITE_SCRIPT_FIELD(Vector2, glm::vec2);
							WRITE_SCRIPT_FIELD(Vector3, glm::vec3);
							WRITE_SCRIPT_FIELD(Vector4, glm::vec4);
							WRITE_SCRIPT_FIELD(Entity,  UUID);
						}
						out << YAML::EndMap; // Field
					}
					out << YAML::EndSeq;
					out << YAML::EndMap; // ManagedComponent
				}
				out << YAML::EndSeq;
			}
		}

		if (entity.HasComponent<SpriteRendererComponent>())
		{
			out << YAML::Key << "SpriteRendererComponent";
			out << YAML::BeginMap; // SpriteRendererComponent

			auto& spriteRendererComponent = entity.GetComponent<SpriteRendererComponent>();
			out << YAML::Key << "Color" << YAML::Value << spriteRendererComponent.Color;
			out << YAML::Key << "TextureHandle" << YAML::Value << spriteRendererComponent.Texture;
			out << YAML::Key << "TilingFactor" << YAML::Value << spriteRendererComponent.TilingFactor;

			out << YAML::EndMap; // SpriteRendererComponent
		}
		
		if (entity.HasComponent<CircleRendererComponent>())
		{
			out << YAML::Key << "CircleRendererComponent";
			out << YAML::BeginMap; // CircleRendererComponent

			auto& circleRendererComponent = entity.GetComponent<CircleRendererComponent>();
			out << YAML::Key << "Color" << YAML::Value << circleRendererComponent.Color;
			out << YAML::Key << "Thickness" << YAML::Value << circleRendererComponent.Thickness;
			out << YAML::Key << "Fade" << YAML::Value << circleRendererComponent.Fade;

			out << YAML::EndMap; // CircleRendererComponent
		}

		if (entity.HasComponent<MeshComponent>())
		{
			out << YAML::Key << "MeshComponent";
			out << YAML::BeginMap; // MeshComponent

			auto& meshComponent = entity.GetComponent<MeshComponent>();
			out << YAML::Key << "PrimitiveType" << YAML::Value << Utils::PrimitiveTypeToString(meshComponent.Type);
			out << YAML::Key << "Material" << YAML::Value << (uint64_t)meshComponent.Material;

			out << YAML::EndMap; // MeshComponent
		}

		if (entity.HasComponent<LightComponent>())
		{
			out << YAML::Key << "LightComponent";
			out << YAML::BeginMap; // LightComponent

			auto& lightComponent = entity.GetComponent<LightComponent>();
			out << YAML::Key << "Type" << YAML::Value << Utils::LightTypeToString(lightComponent.Type);
			out << YAML::Key << "Color" << YAML::Value << lightComponent.Color;
			out << YAML::Key << "Intensity" << YAML::Value << lightComponent.Intensity;
			out << YAML::Key << "Range" << YAML::Value << lightComponent.Range;
			out << YAML::Key << "InnerCutoff" << YAML::Value << lightComponent.InnerCutoff;
			out << YAML::Key << "OuterCutoff" << YAML::Value << lightComponent.OuterCutoff;

			out << YAML::EndMap; // LightComponent
		}

		if (entity.HasComponent<Rigidbody2DComponent>())
		{
			out << YAML::Key << "Rigidbody2DComponent";
			out << YAML::BeginMap; // Rigidbody2DComponent

			auto& rb2dComponent = entity.GetComponent<Rigidbody2DComponent>();
			out << YAML::Key << "BodyType" << YAML::Value << Utils::RigidBody2DBodyTypeToString(rb2dComponent.Type);
			out << YAML::Key << "FixedRotation" << YAML::Value << rb2dComponent.FixedRotation;

			out << YAML::EndMap; // Rigidbody2DComponent
		}

		if (entity.HasComponent<BoxCollider2DComponent>())
		{
			out << YAML::Key << "BoxCollider2DComponent";
			out << YAML::BeginMap; // BoxCollider2DComponent

			auto& bc2dComponent = entity.GetComponent<BoxCollider2DComponent>();
			out << YAML::Key << "Offset" << YAML::Value << bc2dComponent.Offset;
			out << YAML::Key << "Size" << YAML::Value << bc2dComponent.Size;
			out << YAML::Key << "Density" << YAML::Value << bc2dComponent.Density;
			out << YAML::Key << "Friction" << YAML::Value << bc2dComponent.Friction;
			out << YAML::Key << "Restitution" << YAML::Value << bc2dComponent.Restitution;
			out << YAML::Key << "RestitutionThreshold" << YAML::Value << entity.GetScene()->GetRestitutionThreshold();

			out << YAML::EndMap; // BoxCollider2DComponent
		}

		if (entity.HasComponent<CircleCollider2DComponent>())
		{
			out << YAML::Key << "CircleCollider2DComponent";
			out << YAML::BeginMap; // CircleCollider2DComponent

			auto& cc2dComponent = entity.GetComponent<CircleCollider2DComponent>();
			out << YAML::Key << "Offset" << YAML::Value << cc2dComponent.Offset;
			out << YAML::Key << "Radius" << YAML::Value << cc2dComponent.Radius;
			out << YAML::Key << "Density" << YAML::Value << cc2dComponent.Density;
			out << YAML::Key << "Friction" << YAML::Value << cc2dComponent.Friction;
			out << YAML::Key << "Restitution" << YAML::Value << cc2dComponent.Restitution;
			out << YAML::Key << "RestitutionThreshold" << YAML::Value << entity.GetScene()->GetRestitutionThreshold();

			out << YAML::EndMap; // CircleCollider2DComponent
		}
		
		if (entity.HasComponent<TextComponent>())
		{
			out << YAML::Key << "TextComponent";
			out << YAML::BeginMap; // TextComponent

			auto& textComponent = entity.GetComponent<TextComponent>();
			out << YAML::Key << "Text" << YAML::Value << textComponent.Text;
			// TODO: textComponent.FontAsset
			out << YAML::Key << "Color" << YAML::Value << textComponent.Color;
			out << YAML::Key << "Kerning" << YAML::Value << textComponent.Kerning;
			out << YAML::Key << "LineSpacing" << YAML::Value << textComponent.LineSpacing;

			out << YAML::EndMap; // TextComponent
		}

		out << YAML::EndMap; // Entity
	}

	void SceneSerializer::SerializeText(const std::filesystem::path& filepath)
	{
		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Scene" << YAML::Value << "Timefall Scene";
		out << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq;
		m_Scene->m_Registry.view<entt::entity>().each([&](auto entityID)
		{
			Entity entity = { entityID, m_Scene.get() };
			if (!entity)
				return;

			SerializeEntity(out, entity);
		});
		out << YAML::EndSeq;
		out << YAML::EndMap;

		std::ofstream fout(filepath);
		fout << out.c_str();
	}

	void SceneSerializer::SerializeBinary(const std::filesystem::path& filepath)
	{
		TF_CORE_ASSERT(false, "Not implemented yet!");
	}

	bool SceneSerializer::DeserializeText(const std::filesystem::path& filepath)
	{
		std::ifstream stream(filepath);
		std::stringstream strStream;
		strStream << stream.rdbuf();

		YAML::Node data;
		
		try 
		{
			data = YAML::Load(strStream.str());
		}
		catch (const YAML::ParserException& e)
		{
			TF_CORE_ERROR("Failed to parse scene file '{0}': {1}", filepath.string(), e.what());
			return false;
		}
		
		if (!data["Scene"])
			return false;

		std::string sceneName = data["Scene"].as<std::string>();
		TF_CORE_TRACE("Deserializing scene '{0}'", sceneName);

		if (auto entities = data["Entities"])
		{
			for (auto entity : entities)
			{
				uint64_t uuid = entity["Entity"].as<uint64_t>();

				std::string name;
				if (auto tagComponent = entity["TagComponent"])
					name = tagComponent["Tag"].as<std::string>();

				TF_CORE_TRACE("Deserialized entity with ID = {0}, name = {1}", uuid, name);

				Entity deserializedEntity = m_Scene->CreateEntityWithUUID(uuid, name);

				if (auto transformComponent = entity["TransformComponent"])
				{
					// Entities always have transforms
					auto& tc = deserializedEntity.GetComponent<TransformComponent>();
					tc.Translation = transformComponent["Position"].as<glm::vec3>();
					tc.Rotation = transformComponent["Rotation"].as<glm::vec3>();
					tc.Scale = transformComponent["Scale"].as<glm::vec3>();
				}

				if (auto relationshipComponent = entity["RelationshipComponent"])
				{
					// UUIDs are position-independent, so Parent/Children read directly — no second
					// pass needed (the referenced entities all exist by the time anything resolves them).
					auto& rc = deserializedEntity.AddComponent<RelationshipComponent>();
					rc.Parent = relationshipComponent["Parent"].as<uint64_t>();

					if (auto children = relationshipComponent["Children"])
					{
						for (auto child : children)
							rc.Children.push_back(child.as<uint64_t>());
					}
				}

				if (auto cameraComponent = entity["CameraComponent"])
				{
					auto& cc = deserializedEntity.AddComponent<CameraComponent>();

					auto cameraProps = cameraComponent["Camera"];
					cc.Camera.SetProjectionType((SceneCamera::ProjectionType)cameraProps["ProjectionType"].as<int>());

					cc.Camera.SetPerspectiveVerticalFOV(cameraProps["PerspectiveFOV"].as<float>());
					cc.Camera.SetPerspectiveNearClip(cameraProps["PerspectiveNear"].as<float>());
					cc.Camera.SetPerspectiveFarClip(cameraProps["PerspectiveFar"].as<float>());

					cc.Camera.SetOrthographicSize(cameraProps["OrthographicSize"].as<float>());
					cc.Camera.SetOrthographicNearClip(cameraProps["OrthographicNear"].as<float>());
					cc.Camera.SetOrthographicFarClip(cameraProps["OrthographicFar"].as<float>());

					cc.Primary = cameraComponent["Primary"].as<bool>();
					cc.FixedAspectRatio = cameraComponent["FixedAspectRatio"].as<bool>();
				}

				if (auto scriptComponent = entity["ScriptComponent"])
				{
					auto& sc = deserializedEntity.AddComponent<ScriptComponent>();
					sc.ModuleName = scriptComponent["ModuleName"].as<std::wstring>();

					auto scriptFields = scriptComponent["ScriptFields"];
					if (scriptFields)
					{
						Ref<ScriptClass> entityClass = ScriptEngine::GetEntityScriptClass(sc.ModuleName);
						if (entityClass)
						{
							const auto& fields = entityClass->GetFields();
							auto& entityFields = ScriptEngine::GetEntityScriptFields(deserializedEntity);

							for (auto scriptField : scriptFields)
							{
								std::wstring name = scriptField["Name"].as<std::wstring>();
								std::wstring typeString = scriptField["Type"].as<std::wstring>();
								ScriptFieldType type = Utils::ScriptFieldTypeFromString(typeString);

								ScriptFieldInstance& fieldInstance = entityFields[name];

								if (fields.find(name) == fields.end())
								{
									TF_CORE_WARN("Script field not found");
									continue;
								}

								fieldInstance.Field = fields.at(name);

								switch (type)
								{
									READ_SCRIPT_FIELD(Float, float);
									READ_SCRIPT_FIELD(Double, double);
									READ_SCRIPT_FIELD(Bool, bool);
									READ_SCRIPT_FIELD(Char, char);
									READ_SCRIPT_FIELD(SByte, int8_t);
									READ_SCRIPT_FIELD(Int16, int16_t);
									READ_SCRIPT_FIELD(Int32, int32_t);
									READ_SCRIPT_FIELD(Int64, int64_t);
									READ_SCRIPT_FIELD(Byte, uint8_t);
									READ_SCRIPT_FIELD(UInt16, uint16_t);
									READ_SCRIPT_FIELD(UInt32, uint32_t);
									READ_SCRIPT_FIELD(UInt64, uint64_t);
									READ_SCRIPT_FIELD(Vector2, glm::vec2);
									READ_SCRIPT_FIELD(Vector3, glm::vec3);
									READ_SCRIPT_FIELD(Vector4, glm::vec4);
									READ_SCRIPT_FIELD(Entity, UUID);
								}
							}
						}
					}
				}

				if (auto managedComponents = entity["ManagedComponents"])
				{
					auto& storage = deserializedEntity.HasComponent<ManagedComponentStorage>()
						? deserializedEntity.GetComponent<ManagedComponentStorage>()
						: deserializedEntity.AddComponent<ManagedComponentStorage>();

					for (auto managedComponent : managedComponents)
					{
						std::wstring typeName = managedComponent["TypeName"].as<std::wstring>();
						ManagedComponentData componentData; // not `data`: the READ_SCRIPT_FIELD macro declares a local `data`

						if (auto fields = managedComponent["Fields"])
						{
							for (auto scriptField : fields)
							{
								std::wstring name = scriptField["Name"].as<std::wstring>();
								std::wstring typeString = scriptField["Type"].as<std::wstring>();
								ScriptFieldType type = Utils::ScriptFieldTypeFromString(typeString);

								ScriptFieldInstance& fieldInstance = componentData.Fields[name];
								fieldInstance.Field = ScriptField{ name, type };

								switch (type)
								{
									READ_SCRIPT_FIELD(Float, float);
									READ_SCRIPT_FIELD(Double, double);
									READ_SCRIPT_FIELD(Bool, bool);
									READ_SCRIPT_FIELD(Char, char);
									READ_SCRIPT_FIELD(SByte, int8_t);
									READ_SCRIPT_FIELD(Int16, int16_t);
									READ_SCRIPT_FIELD(Int32, int32_t);
									READ_SCRIPT_FIELD(Int64, int64_t);
									READ_SCRIPT_FIELD(Byte, uint8_t);
									READ_SCRIPT_FIELD(UInt16, uint16_t);
									READ_SCRIPT_FIELD(UInt32, uint32_t);
									READ_SCRIPT_FIELD(UInt64, uint64_t);
									READ_SCRIPT_FIELD(Vector2, glm::vec2);
									READ_SCRIPT_FIELD(Vector3, glm::vec3);
									READ_SCRIPT_FIELD(Vector4, glm::vec4);
									READ_SCRIPT_FIELD(Entity, UUID);
								}
							}
						}
						storage.Components[typeName] = std::move(componentData);
					}
				}

				if (auto spriteRendererComponent = entity["SpriteRendererComponent"])
				{
					auto& src = deserializedEntity.AddComponent<SpriteRendererComponent>();
					src.Color = spriteRendererComponent["Color"].as<glm::vec4>();

					if (spriteRendererComponent["Texture"])
					{
						// Note: Legacy code
						/*const auto width = spriteRendererComponent["Texture"]["Width"].as<uint32_t>();
						const auto height = spriteRendererComponent["Texture"]["Height"].as<uint32_t>();
						const auto dataFormat = spriteRendererComponent["Texture"]["DataFormat"].as<uint32_t>();
						const auto texturePath = spriteRendererComponent["Texture"]["Path"].as<std::string>();

						auto path = Project::GetAssetFileSystemPath(texturePath);
						src.Texture = Texture2D::Create(path.string());*/
					}

					if (spriteRendererComponent["TextureHandle"])
						src.Texture = spriteRendererComponent["TextureHandle"].as<AssetHandle>();

					if (spriteRendererComponent["TilingFactor"])
						src.TilingFactor = spriteRendererComponent["TilingFactor"].as<float>();
				}
				
				if (auto circleRendererComponent = entity["CircleRendererComponent"])
				{
					auto& crc = deserializedEntity.AddComponent<CircleRendererComponent>();
					crc.Color = circleRendererComponent["Color"].as<glm::vec4>();
					crc.Thickness = circleRendererComponent["Thickness"].as<float>();
					crc.Fade = circleRendererComponent["Fade"].as<float>();
				}

				if (auto meshComponent = entity["MeshComponent"])
				{
					auto& mc = deserializedEntity.AddComponent<MeshComponent>();
					mc.Type = Utils::PrimitiveTypeFromString(meshComponent["PrimitiveType"].as<std::string>());
					if (auto mat = meshComponent["Material"])
						mc.Material = mat.as<uint64_t>();
				}

				if (auto lightComponent = entity["LightComponent"])
				{
					auto& lc = deserializedEntity.AddComponent<LightComponent>();
					lc.Type        = Utils::LightTypeFromString(lightComponent["Type"].as<std::string>());
					lc.Color       = lightComponent["Color"].as<glm::vec3>();
					lc.Intensity   = lightComponent["Intensity"].as<float>();
					lc.Range       = lightComponent["Range"].as<float>();
					lc.InnerCutoff = lightComponent["InnerCutoff"].as<float>();
					lc.OuterCutoff = lightComponent["OuterCutoff"].as<float>();
				}

				auto rigidbody2DComponent = entity["Rigidbody2DComponent"];
				if (rigidbody2DComponent)
				{
					auto& rb2d = deserializedEntity.AddComponent<Rigidbody2DComponent>();
					rb2d.Type = Utils::RigidBody2DBodyTypeFromString(rigidbody2DComponent["BodyType"].as<std::string>());
					rb2d.FixedRotation = rigidbody2DComponent["FixedRotation"].as<bool>();
				}

				auto boxCollider2DComponent = entity["BoxCollider2DComponent"];
				if (boxCollider2DComponent)
				{
					auto& bc2d = deserializedEntity.AddComponent<BoxCollider2DComponent>();
					bc2d.Offset = boxCollider2DComponent["Offset"].as<glm::vec2>();
					bc2d.Size = boxCollider2DComponent["Size"].as<glm::vec2>();
					bc2d.Density = boxCollider2DComponent["Density"].as<float>();
					bc2d.Friction = boxCollider2DComponent["Friction"].as<float>();
					bc2d.Restitution = boxCollider2DComponent["Restitution"].as<float>();
					deserializedEntity.GetScene()->SetRestitutionThreshold(boxCollider2DComponent["RestitutionThreshold"].as<float>());
				}

				auto circleCollider2DComponent = entity["CircleCollider2DComponent"];
				if (circleCollider2DComponent)
				{
					auto& cc2d = deserializedEntity.AddComponent<CircleCollider2DComponent>();
					cc2d.Offset = circleCollider2DComponent["Offset"].as<glm::vec2>();
					cc2d.Radius = circleCollider2DComponent["Radius"].as<float>();
					cc2d.Density = circleCollider2DComponent["Density"].as<float>();
					cc2d.Friction = circleCollider2DComponent["Friction"].as<float>();
					cc2d.Restitution = circleCollider2DComponent["Restitution"].as<float>();
					deserializedEntity.GetScene()->SetRestitutionThreshold(circleCollider2DComponent["RestitutionThreshold"].as<float>());
				}
				
				auto textComponent = entity["TextComponent"];
				if (textComponent)
				{
					auto& tc = deserializedEntity.AddComponent<TextComponent>();
					tc.Text = textComponent["Text"].as<std::string>();
					// TODO: tc.FontAsset
					tc.Color = textComponent["Color"].as<glm::vec4>();
					tc.Kerning = textComponent["Kerning"].as<float>();
					tc.LineSpacing = textComponent["LineSpacing"].as<float>();
				}
			}
		}

		return true;
	}

	bool SceneSerializer::DeserializeBinary(const std::filesystem::path& filepath)
	{
		TF_CORE_ASSERT(false, "Not implemented yet!");
		return false;
	}
}