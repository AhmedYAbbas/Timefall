#pragma once

#include "Timefall/Core/Core.h"
#include "Timefall/Asset/Asset.h"

#include "Timefall/RHI/GpuBuffer.h"

#include <glm/glm.hpp>
#include <vector>
#include <string>

namespace Timefall
{
	struct MeshVertex
	{
		glm::vec3 Position;
		glm::vec3 Normal;
		glm::vec2 TexCoord;
		glm::vec3 Tangent;
		glm::vec3 Bitangent;
	};

	// A range within the MeshSource's combined buffers, drawn as one DrawIndexed.
	struct Submesh
	{
		uint32_t BaseVertex = 0;
		uint32_t BaseIndex = 0;
		uint32_t IndexCount = 0;
		uint32_t MaterialIndex = 0; // index into the import's material list (transient; used at import)
		std::string Name;

		// Local-space axis-aligned bounds of this submesh's vertices (for selection/culling).
		glm::vec3 MinBounds{0.0f};
		glm::vec3 MaxBounds{0.0f};
	};

	// Retained only for the legacy MeshComponent deserializer shim (maps to reserved built-in handles).
	enum class PrimitiveType : uint8_t { Cube = 0, Sphere = 1, Plane = 2 };

	class TF_API MeshSource : public Asset
	{
	public:
		MeshSource(const std::vector<MeshVertex>& vertices, const std::vector<uint32_t>& indices, const std::vector<Submesh>& submeshes);

		const std::vector<Submesh>& GetSubmeshes() const { return m_Submeshes; }

		const Ref<RHI::GpuBuffer> GetVertexBuffer() const { return m_VertexBuffer; }
		const Ref<RHI::GpuBuffer> GetIndexBuffer() const { return m_IndexBuffer; }
		bool HasGpuBuffers() const { return m_VertexBuffer && m_VertexBuffer->IsValid() && m_IndexBuffer && m_IndexBuffer->IsValid(); }

		static AssetType GetStaticType() { return AssetType::Mesh; }
		virtual AssetType GetType() const override { return GetStaticType(); }

		static Ref<MeshSource> Create(
			const std::vector<MeshVertex>& vertices, const std::vector<uint32_t>& indices, const std::vector<Submesh>& submeshes);

		// Built-in primitives (centered at origin, ~unit size, Y-up). Each is a single-submesh source.
		static Ref<MeshSource> CreateCube();
		static Ref<MeshSource> CreateSphere(uint32_t sectorCount = 32, uint32_t stackCount = 16, float radius = 0.5f);
		static Ref<MeshSource> CreatePlane();

	private:
		std::vector<Submesh> m_Submeshes;

		Ref<RHI::GpuBuffer> m_VertexBuffer;
		Ref<RHI::GpuBuffer> m_IndexBuffer;
	};

	// String <-> enum helpers, retained for the legacy serializer shim.
	namespace Utils
	{
		static const char* PrimitiveTypeToString(PrimitiveType type)
		{
			switch (type)
			{
				case PrimitiveType::Cube: return "Cube";
				case PrimitiveType::Sphere: return "Sphere";
				case PrimitiveType::Plane: return "Plane";
			}
			return "Cube";
		}

		static PrimitiveType PrimitiveTypeFromString(const std::string& type)
		{
			if (type == "Sphere")
				return PrimitiveType::Sphere;
			if (type == "Plane")
				return PrimitiveType::Plane;
			return PrimitiveType::Cube;
		}
	}
}
