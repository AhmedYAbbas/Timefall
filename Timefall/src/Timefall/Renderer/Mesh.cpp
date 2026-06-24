#include "tfpch.h"

#include "Timefall/Renderer/Mesh.h"
#include "Timefall/Renderer/Buffer.h"

#include <glm/gtc/constants.hpp>

namespace Timefall
{
	MeshSource::MeshSource(const std::vector<MeshVertex>& vertices, const std::vector<uint32_t>& indices,
		const std::vector<Submesh>& submeshes)
		: m_Submeshes(submeshes)
	{
		m_VertexArray = VertexArray::Create();
		m_VertexArray->Bind();

		Ref<VertexBuffer> vbo = VertexBuffer::Create(
			(float*)vertices.data(),
			(uint32_t)(vertices.size() * sizeof(MeshVertex)));
		vbo->SetLayout({
			{ ShaderDataType::Float3, "a_Position"  },
			{ ShaderDataType::Float3, "a_Normal"    },
			{ ShaderDataType::Float2, "a_TexCoord"  },
			{ ShaderDataType::Float3, "a_Tangent"   },
			{ ShaderDataType::Float3, "a_Bitangent" },
		});
		m_VertexArray->AddVertexBuffer(vbo);

		Ref<IndexBuffer> ibo = IndexBuffer::Create((uint32_t*)indices.data(), (uint32_t)indices.size());
		m_VertexArray->SetIndexBuffer(ibo);
	}

	Ref<MeshSource> MeshSource::Create(const std::vector<MeshVertex>& vertices, const std::vector<uint32_t>& indices,
		const std::vector<Submesh>& submeshes)
	{
		return CreateRef<MeshSource>(vertices, indices, submeshes);
	}

	static Submesh FullRangeSubmesh(const std::vector<MeshVertex>& vertices, uint32_t indexCount)
	{
		Submesh sm;
		sm.BaseVertex = 0;
		sm.BaseIndex = 0;
		sm.IndexCount = indexCount;
		sm.MaterialIndex = 0;
		sm.Name = "Primitive";

		glm::vec3 mn(std::numeric_limits<float>::max());
		glm::vec3 mx(std::numeric_limits<float>::lowest());
		for (const MeshVertex& v : vertices)
		{
			mn = glm::min(mn, v.Position);
			mx = glm::max(mx, v.Position);
		}
		sm.MinBounds = mn;
		sm.MaxBounds = mx;
		return sm;
	}

	// Accumulate per-vertex tangents from triangle edges and UV deltas, then orthonormalize.
	// Used by the built-in primitives; imported meshes get tangents from Assimp instead.
	static void ComputeTangents(std::vector<MeshVertex>& verts, const std::vector<uint32_t>& indices)
	{
		for (MeshVertex& v : verts) { v.Tangent = glm::vec3(0.0f); v.Bitangent = glm::vec3(0.0f); }

		for (size_t i = 0; i + 2 < indices.size(); i += 3)
		{
			MeshVertex& v0 = verts[indices[i + 0]];
			MeshVertex& v1 = verts[indices[i + 1]];
			MeshVertex& v2 = verts[indices[i + 2]];

			glm::vec3 e1 = v1.Position - v0.Position;
			glm::vec3 e2 = v2.Position - v0.Position;
			glm::vec2 d1 = v1.TexCoord - v0.TexCoord;
			glm::vec2 d2 = v2.TexCoord - v0.TexCoord;

			float denom = d1.x * d2.y - d2.x * d1.y;
			float f = glm::abs(denom) < 1e-8f ? 0.0f : 1.0f / denom;

			glm::vec3 t = f * (d2.y * e1 - d1.y * e2);
			glm::vec3 b = f * (d1.x * e2 - d2.x * e1);

			v0.Tangent += t; v1.Tangent += t; v2.Tangent += t;
			v0.Bitangent += b; v1.Bitangent += b; v2.Bitangent += b;
		}

		for (MeshVertex& v : verts)
		{
			glm::vec3 n = v.Normal;
			glm::vec3 t = v.Tangent - n * glm::dot(n, v.Tangent);   // Gram-Schmidt
			v.Tangent = glm::length(t) > 1e-6f ? glm::normalize(t)
			                                   : glm::vec3(1.0f, 0.0f, 0.0f);
			v.Bitangent = glm::cross(n, v.Tangent) *
				(glm::dot(glm::cross(n, v.Tangent), v.Bitangent) < 0.0f ? -1.0f : 1.0f);
		}
	}

	Ref<MeshSource> MeshSource::CreateCube()
	{
		std::vector<MeshVertex> vertices = {
			// Front (+Z)
			{ {-0.5f,-0.5f, 0.5f}, { 0, 0, 1}, {0,0} },
			{ { 0.5f,-0.5f, 0.5f}, { 0, 0, 1}, {1,0} },
			{ { 0.5f, 0.5f, 0.5f}, { 0, 0, 1}, {1,1} },
			{ {-0.5f, 0.5f, 0.5f}, { 0, 0, 1}, {0,1} },
			// Back (-Z)
			{ { 0.5f,-0.5f,-0.5f}, { 0, 0,-1}, {0,0} },
			{ {-0.5f,-0.5f,-0.5f}, { 0, 0,-1}, {1,0} },
			{ {-0.5f, 0.5f,-0.5f}, { 0, 0,-1}, {1,1} },
			{ { 0.5f, 0.5f,-0.5f}, { 0, 0,-1}, {0,1} },
			// Left (-X)
			{ {-0.5f,-0.5f,-0.5f}, {-1, 0, 0}, {0,0} },
			{ {-0.5f,-0.5f, 0.5f}, {-1, 0, 0}, {1,0} },
			{ {-0.5f, 0.5f, 0.5f}, {-1, 0, 0}, {1,1} },
			{ {-0.5f, 0.5f,-0.5f}, {-1, 0, 0}, {0,1} },
			// Right (+X)
			{ { 0.5f,-0.5f, 0.5f}, { 1, 0, 0}, {0,0} },
			{ { 0.5f,-0.5f,-0.5f}, { 1, 0, 0}, {1,0} },
			{ { 0.5f, 0.5f,-0.5f}, { 1, 0, 0}, {1,1} },
			{ { 0.5f, 0.5f, 0.5f}, { 1, 0, 0}, {0,1} },
			// Top (+Y)
			{ {-0.5f, 0.5f, 0.5f}, { 0, 1, 0}, {0,0} },
			{ { 0.5f, 0.5f, 0.5f}, { 0, 1, 0}, {1,0} },
			{ { 0.5f, 0.5f,-0.5f}, { 0, 1, 0}, {1,1} },
			{ {-0.5f, 0.5f,-0.5f}, { 0, 1, 0}, {0,1} },
			// Bottom (-Y)
			{ {-0.5f,-0.5f,-0.5f}, { 0,-1, 0}, {0,0} },
			{ { 0.5f,-0.5f,-0.5f}, { 0,-1, 0}, {1,0} },
			{ { 0.5f,-0.5f, 0.5f}, { 0,-1, 0}, {1,1} },
			{ {-0.5f,-0.5f, 0.5f}, { 0,-1, 0}, {0,1} },
		};

		std::vector<uint32_t> indices;
		indices.reserve(36);
		for (uint32_t face = 0; face < 6; face++)
		{
			uint32_t v = face * 4;
			indices.insert(indices.end(), { v + 0, v + 1, v + 2, v + 2, v + 3, v + 0 });
		}

		ComputeTangents(vertices, indices);
		return Create(vertices, indices, { FullRangeSubmesh(vertices, (uint32_t)indices.size()) });
	}

	Ref<MeshSource> MeshSource::CreateSphere(uint32_t sectorCount, uint32_t stackCount, float radius)
	{
		std::vector<MeshVertex> vertices;
		constexpr float PI = glm::pi<float>();

		// (stackCount+1) rows x (sectorCount+1) columns, Y-up.
		for (uint32_t i = 0; i <= stackCount; i++)
		{
			float stackAngle = PI / 2.0f - (float)i * (PI / (float)stackCount); // +pi/2 .. -pi/2
			float xz = radius * cosf(stackAngle);
			float y  = radius * sinf(stackAngle);

			for (uint32_t j = 0; j <= sectorCount; j++)
			{
				float sectorAngle = (float)j * (2.0f * PI / (float)sectorCount);
				float x = xz * cosf(sectorAngle);
				float z = xz * sinf(sectorAngle);

				MeshVertex vert;
				vert.Position = { x, y, z };
				vert.Normal   = glm::normalize(glm::vec3(x, y, z));
				vert.TexCoord = { (float)j / (float)sectorCount, (float)i / (float)stackCount };
				vertices.push_back(vert);
			}
		}

		std::vector<uint32_t> indices;
		for (uint32_t i = 0; i < stackCount; i++)
		{
			uint32_t k1 = i * (sectorCount + 1);
			uint32_t k2 = k1 + sectorCount + 1;
			for (uint32_t j = 0; j < sectorCount; j++, k1++, k2++)
			{
				if (i != 0)
					indices.insert(indices.end(), { k1, k1 + 1, k2 });
				if (i != (stackCount - 1))
					indices.insert(indices.end(), { k1 + 1, k2 + 1, k2 });
			}
		}

		ComputeTangents(vertices, indices);
		return Create(vertices, indices, { FullRangeSubmesh(vertices, (uint32_t)indices.size()) });
	}

	Ref<MeshSource> MeshSource::CreatePlane()
	{
		std::vector<MeshVertex> vertices = {
			{ {-0.5f, 0.0f,-0.5f}, {0,1,0}, {0,0} },
			{ { 0.5f, 0.0f,-0.5f}, {0,1,0}, {1,0} },
			{ { 0.5f, 0.0f, 0.5f}, {0,1,0}, {1,1} },
			{ {-0.5f, 0.0f, 0.5f}, {0,1,0}, {0,1} },
		};
		// Wound so the geometric front face matches the +Y normal (consistent with the cube),
		// which front-face shadow culling relies on.
		std::vector<uint32_t> indices = { 0, 2, 1, 2, 0, 3 };
		ComputeTangents(vertices, indices);
		return Create(vertices, indices, { FullRangeSubmesh(vertices, (uint32_t)indices.size()) });
	}
}
