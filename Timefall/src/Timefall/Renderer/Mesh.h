#pragma once

#include "Timefall/Core/Core.h"
#include "Timefall/Renderer/VertexArray.h"

#include <glm/glm.hpp>
#include <vector>

namespace Timefall
{
	struct MeshVertex
	{
		glm::vec3 Position;
		glm::vec3 Normal;
		glm::vec2 TexCoord;
	};

	// Interim selector for built-in geometry (9.1). Replaced by real mesh assets in 9.4.
	enum class PrimitiveType : uint8_t
	{
		Cube = 0,
		Sphere = 1,
		Plane = 2
	};

	class TF_API Mesh
	{
	public:
		Mesh(const std::vector<MeshVertex>& vertices, const std::vector<uint32_t>& indices);

		const Ref<VertexArray>& GetVertexArray() const { return m_VertexArray; }
		uint32_t GetIndexCount() const { return m_IndexCount; }

		static Ref<Mesh> Create(const std::vector<MeshVertex>& vertices, const std::vector<uint32_t>& indices);

		// Built-in primitives (centered at origin, ~unit size, Y-up).
		static Ref<Mesh> CreateCube();
		static Ref<Mesh> CreateSphere(uint32_t sectorCount = 32, uint32_t stackCount = 16, float radius = 0.5f);
		static Ref<Mesh> CreatePlane();

	private:
		Ref<VertexArray> m_VertexArray;
		uint32_t m_IndexCount = 0;
	};

	// String <-> enum helpers for serialization (mirrors Utils::RigidBody2DBodyType* in Physics2D.h).
	namespace Utils
	{
		static const char* PrimitiveTypeToString(PrimitiveType type)
		{
			switch (type)
			{
				case PrimitiveType::Cube:   return "Cube";
				case PrimitiveType::Sphere: return "Sphere";
				case PrimitiveType::Plane:  return "Plane";
			}
			TF_CORE_ASSERT(false, "Unknown PrimitiveType");
			return "Cube";
		}

		static PrimitiveType PrimitiveTypeFromString(const std::string& type)
		{
			if (type == "Cube")   return PrimitiveType::Cube;
			if (type == "Sphere") return PrimitiveType::Sphere;
			if (type == "Plane")  return PrimitiveType::Plane;

			TF_CORE_ASSERT(false, "Unknown PrimitiveType string");
			return PrimitiveType::Cube;
		}
	}
}
