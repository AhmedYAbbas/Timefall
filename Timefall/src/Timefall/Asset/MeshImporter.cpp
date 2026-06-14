#include "tfpch.h"
#include "MeshImporter.h"

#include "Timefall/Project/Project.h"
#include "Timefall/Scene/Scene.h"
#include "Timefall/Scene/Components.h"
#include "Timefall/Renderer/Material.h"
#include "Timefall/Asset/MaterialImporter.h"
#include "Timefall/Asset/EditorAssetManager.h"
#include "Timefall/Math/Math.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <glm/glm.hpp>

namespace Timefall
{
	static constexpr uint32_t s_PostProcessFlags =
		aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs |
		aiProcess_JoinIdenticalVertices | aiProcess_OptimizeMeshes;

	Ref<MeshSource> MeshImporter::ImportMesh(AssetHandle handle, const AssetMetadata& metadata)
	{
		return LoadMesh(Project::GetAssetDirectory() / metadata.FilePath);
	}

	Ref<MeshSource> MeshImporter::LoadMesh(const std::filesystem::path& path)
	{
		Assimp::Importer importer;
		const aiScene* scene = importer.ReadFile(path.string(), s_PostProcessFlags);
		if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode)
		{
			TF_CORE_ERROR("MeshImporter::LoadMesh - Assimp failed for {0}: {1}", path.string(), importer.GetErrorString());
			return nullptr;
		}

		std::vector<MeshVertex> vertices;
		std::vector<uint32_t>   indices;
		std::vector<Submesh>    submeshes;

		for (uint32_t m = 0; m < scene->mNumMeshes; m++)
		{
			const aiMesh* mesh = scene->mMeshes[m];

			Submesh sm;
			sm.BaseVertex = (uint32_t)vertices.size();
			sm.BaseIndex  = (uint32_t)indices.size();
			sm.MaterialIndex = mesh->mMaterialIndex;
			sm.Name = mesh->mName.C_Str();

			glm::vec3 mn(std::numeric_limits<float>::max());
			glm::vec3 mx(std::numeric_limits<float>::lowest());

			for (uint32_t v = 0; v < mesh->mNumVertices; v++)
			{
				MeshVertex vert;
				vert.Position = { mesh->mVertices[v].x, mesh->mVertices[v].y, mesh->mVertices[v].z };
				vert.Normal   = mesh->HasNormals()
					? glm::vec3(mesh->mNormals[v].x, mesh->mNormals[v].y, mesh->mNormals[v].z)
					: glm::vec3(0.0f, 1.0f, 0.0f);
				vert.TexCoord = mesh->HasTextureCoords(0)
					? glm::vec2(mesh->mTextureCoords[0][v].x, mesh->mTextureCoords[0][v].y)
					: glm::vec2(0.0f);
				vertices.push_back(vert);

				mn = glm::min(mn, vert.Position);
				mx = glm::max(mx, vert.Position);
			}

			sm.MinBounds = mn;
			sm.MaxBounds = mx;

			uint32_t indexCount = 0;
			for (uint32_t f = 0; f < mesh->mNumFaces; f++)
			{
				const aiFace& face = mesh->mFaces[f];
				for (uint32_t i = 0; i < face.mNumIndices; i++)
				{
					indices.push_back(face.mIndices[i]);   // base-vertex offset applied at draw
					indexCount++;
				}
			}
			sm.IndexCount = indexCount;
			submeshes.push_back(sm);
		}

		if (vertices.empty() || indices.empty())
			return nullptr;

		return MeshSource::Create(vertices, indices, submeshes);
	}

	namespace
	{
		// Assimp matrices are row-major; GLM is column-major -> transpose on convert.
		glm::mat4 ToGlm(const aiMatrix4x4& m)
		{
			return glm::mat4(
				m.a1, m.b1, m.c1, m.d1,
				m.a2, m.b2, m.c2, m.d2,
				m.a3, m.b3, m.c3, m.d3,
				m.a4, m.b4, m.c4, m.d4);
		}
	}

	Entity MeshImporter::ImportModel(const Ref<Scene>& scene, const std::filesystem::path& modelPath)
	{
		auto assetManager = Project::GetActive()->GetEditorAssetManager();
		auto relModelPath = std::filesystem::relative(modelPath, Project::GetAssetDirectory());

		// 1. Register the mesh file as an asset (re-imported via Assimp on load).
		AssetHandle meshHandle = assetManager->ImportAsset(relModelPath);
		if (meshHandle == 0)
		{
			TF_CORE_ERROR("ImportModel - failed to register mesh asset: {0}", modelPath.string());
			return {};
		}

		// 2. Run Assimp once for materials + node hierarchy.
		Assimp::Importer importer;
		const aiScene* ascene = importer.ReadFile(modelPath.string(), s_PostProcessFlags);
		if (!ascene || !ascene->mRootNode)
		{
			TF_CORE_ERROR("ImportModel - Assimp failed: {0}", importer.GetErrorString());
			return {};
		}

		// 3. Generate one .tfmat per aiMaterial.
		std::filesystem::path matDir = Project::GetAssetDirectory() / "Materials";
		std::filesystem::create_directories(matDir);
		std::string modelStem = modelPath.stem().string();

		std::vector<AssetHandle> materialHandles(ascene->mNumMaterials, 0);
		for (uint32_t i = 0; i < ascene->mNumMaterials; i++)
		{
			const aiMaterial* aimat = ascene->mMaterials[i];

			Ref<Material> material = CreateRef<Material>();
			aiColor3D diffuse(1.0f, 1.0f, 1.0f), specular(1.0f, 1.0f, 1.0f);
			if (aimat->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse) == AI_SUCCESS)
				material->DiffuseColor = { diffuse.r, diffuse.g, diffuse.b };
			if (aimat->Get(AI_MATKEY_COLOR_SPECULAR, specular) == AI_SUCCESS)
				material->SpecularColor = { specular.r, specular.g, specular.b };

			aiString aiName;
			aimat->Get(AI_MATKEY_NAME, aiName);
			std::string matName = aiName.length ? aiName.C_Str() : ("mat" + std::to_string(i));

			auto relMatPath = std::filesystem::path("Materials") / (modelStem + "_" + matName + ".tfmat");
			MaterialImporter::Serialize(Project::GetAssetDirectory() / relMatPath, material);
			materialHandles[i] = assetManager->ImportAsset(relMatPath);
		}

		// 4. Walk the node tree, spawning a parented entity hierarchy.
		std::function<Entity(const aiNode*, Entity)> spawn =
			[&](const aiNode* node, Entity parent) -> Entity
		{
			Entity entity = scene->CreateEntity(node->mName.length ? node->mName.C_Str() : "Node");
			if (parent)
				scene->SetParent(entity, parent);

			glm::vec3 t, r, s;
			Math::DecomposeTransform(ToGlm(node->mTransformation), t, r, s);
			auto& tc = entity.GetComponent<TransformComponent>();
			tc.Translation = t; tc.Rotation = r; tc.Scale = s;

			// Each mesh on this node = one submesh; multiple meshes spawn child entities.
			for (uint32_t m = 0; m < node->mNumMeshes; m++)
			{
				uint32_t submeshIndex = node->mMeshes[m];
				Entity meshEntity = entity;
				if (node->mNumMeshes > 1)
				{
					meshEntity = scene->CreateEntity(std::string(node->mName.C_Str()) + "_" + std::to_string(m));
					scene->SetParent(meshEntity, entity);
				}
				auto& mc = meshEntity.AddComponent<MeshComponent>();
				mc.Mesh = meshHandle;
				mc.Submesh = submeshIndex;
				uint32_t matIndex = ascene->mMeshes[submeshIndex]->mMaterialIndex;
				mc.Material = (matIndex < materialHandles.size()) ? materialHandles[matIndex] : (AssetHandle)0;
			}

			for (uint32_t c = 0; c < node->mNumChildren; c++)
				spawn(node->mChildren[c], entity);

			return entity;
		};

		return spawn(ascene->mRootNode, Entity{});
	}
}
