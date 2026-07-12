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
#include <assimp/GltfMaterial.h>

#include <glm/glm.hpp>

namespace Timefall
{
	static constexpr uint32_t s_PostProcessFlags =
		aiProcess_Triangulate | aiProcess_GenSmoothNormals | /* aiProcess_FlipUVs | */
		aiProcess_JoinIdenticalVertices | aiProcess_OptimizeMeshes |
		aiProcess_CalcTangentSpace;

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

		// Flatten every aiMesh in the scene into one combined vertex/index buffer. Submesh i maps to
		// scene->mMeshes[i], so node->mMeshes[] indices used by the entity-tree walk line up with
		// these submeshes — as long as the walk reads the SAME aiScene this was built from.
		Ref<MeshSource> BuildMeshSource(const aiScene* scene)
		{
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
					if (mesh->HasTangentsAndBitangents())
					{
						vert.Tangent   = { mesh->mTangents[v].x,   mesh->mTangents[v].y,   mesh->mTangents[v].z };
						vert.Bitangent = { mesh->mBitangents[v].x, mesh->mBitangents[v].y, mesh->mBitangents[v].z };
					}
					else
					{
						vert.Tangent   = glm::vec3(0.0f);
						vert.Bitangent = glm::vec3(0.0f);
					}
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

		// Turn one of a material's texture references into a Texture2D asset handle. Handles only
		// external image files that live under the project asset directory; embedded textures
		// (FBX/GLB '*N' refs) are out of scope and skipped. Returns 0 on any miss, which Material
		// interprets as "no map — use the flat color". The cache dedups repeated paths within a
		// single import so two materials (or the diffuse+specular slots) sharing a file don't
		// register the same texture twice.
		AssetHandle ImportTextureSlot(const aiMaterial* aimat, aiTextureType type,
			const std::filesystem::path& modelDir, EditorAssetManager* assetManager,
			std::unordered_map<std::string, AssetHandle>& cache)
		{
			// glTF/PBR exporters store base color under BASE_COLOR rather than DIFFUSE.
			if (type == aiTextureType_DIFFUSE && aimat->GetTextureCount(type) == 0)
				type = aiTextureType_BASE_COLOR;

			if (aimat->GetTextureCount(type) == 0)
				return 0;

			aiString aiPath;
			if (aimat->GetTexture(type, 0, &aiPath) != AI_SUCCESS || aiPath.length == 0)
				return 0;

			std::string pathStr = aiPath.C_Str();
			if (!pathStr.empty() && pathStr[0] == '*')
			{
				TF_CORE_WARN("ImportModel - embedded texture '{0}' skipped (out of scope).", pathStr);
				return 0;
			}

			std::filesystem::path fullPath = modelDir / pathStr;
			if (!std::filesystem::exists(fullPath))
			{
				TF_CORE_WARN("ImportModel - texture file not found: {0}", fullPath.string());
				return 0;
			}

			std::filesystem::path rel = std::filesystem::relative(fullPath, Project::GetAssetDirectory());
			std::string key = rel.generic_string();
			if (key.empty() || key.rfind("..", 0) == 0)
			{
				TF_CORE_WARN("ImportModel - texture '{0}' is outside the asset directory; skipping.", fullPath.string());
				return 0;
			}

			if (cache.contains(key))
				return cache[key];

			AssetHandle handle = assetManager->ImportAsset(rel);
			cache[key] = handle;
			return handle;
		}

		// Normal maps land under different Assimp slots by format: glTF/FBX use NORMALS,
		// OBJ's `map_bump`/`bump` lands under HEIGHT. Try both.
		AssetHandle ImportNormalSlot(const aiMaterial* aimat, const std::filesystem::path& modelDir,
			EditorAssetManager* assetManager, std::unordered_map<std::string, AssetHandle>& cache)
		{
			AssetHandle h = ImportTextureSlot(aimat, aiTextureType_NORMALS, modelDir, assetManager, cache);
			if (h != 0)
				return h;
			return ImportTextureSlot(aimat, aiTextureType_HEIGHT, modelDir, assetManager, cache);
		}
	}

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

		Ref<MeshSource> source = BuildMeshSource(scene);
		if (!source)
			TF_CORE_ERROR("MeshImporter::LoadMesh - no geometry built from {0} ({1} meshes reported by Assimp)",
				path.string(), scene->mNumMeshes);
		return source;
	}

	Entity MeshImporter::ImportModel(const Ref<Scene>& scene, const std::filesystem::path& modelPath)
	{
		auto assetManager = Project::GetActive()->GetEditorAssetManager();
		auto relModelPath = std::filesystem::relative(modelPath, Project::GetAssetDirectory());

		// Import once: the SAME aiScene drives geometry, materials, and the node-tree walk. This keeps
		// submesh indices aligned with the rendered MeshSource (a second independent Assimp import can
		// order meshes differently, making every node draw the wrong submesh) and avoids parsing the
		// source file twice.
		Assimp::Importer importer;
		const aiScene* ascene = importer.ReadFile(modelPath.string(), s_PostProcessFlags);
		if (!ascene || (ascene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !ascene->mRootNode)
		{
			TF_CORE_ERROR("ImportModel - Assimp failed/incomplete for {0}: {1}", modelPath.string(), importer.GetErrorString());
			return {};
		}

		TF_CORE_INFO("ImportModel '{0}': {1} meshes, {2} materials, root '{3}' with {4} children",
			modelPath.filename().string(), ascene->mNumMeshes, ascene->mNumMaterials,
			ascene->mRootNode->mName.C_Str(), ascene->mRootNode->mNumChildren);

		// Build geometry from this scene and register it as the disk-backed mesh asset. The rendered
		// MeshSource is now the exact one the walk below indexes into.
		Ref<MeshSource> meshSource = BuildMeshSource(ascene);
		if (!meshSource)
		{
			TF_CORE_ERROR("ImportModel - no geometry in {0} (Assimp reported {1} meshes). Aborting import.",
				modelPath.string(), ascene->mNumMeshes);
			return {};
		}

		AssetHandle meshHandle = assetManager->ImportLoadedAsset(relModelPath, meshSource);
		if (!assetManager->IsAssetHandleValid(meshHandle))
		{
			TF_CORE_ERROR("ImportModel - failed to register mesh asset: {0}", modelPath.string());
			return {};
		}

		// Generate one .tfmat per aiMaterial.
		std::filesystem::path matDir = Project::GetAssetDirectory() / "Materials";
		std::filesystem::create_directories(matDir);
		std::string modelStem = modelPath.stem().string();

		// Textures are referenced relative to the model file; dedup repeated paths across materials.
		std::filesystem::path modelDir = modelPath.parent_path();
		std::unordered_map<std::string, AssetHandle> texCache;

		std::vector<AssetHandle> materialHandles(ascene->mNumMaterials, 0);
		for (uint32_t i = 0; i < ascene->mNumMaterials; i++)
		{
			const aiMaterial* aimat = ascene->mMaterials[i];

			Ref<Material> material = CreateRef<Material>();

			// Base color: glTF/PBR exporters use BASE_COLOR; fall back to legacy DIFFUSE.
			aiColor4D baseColor(1.0f, 1.0f, 1.0f, 1.0f);
			if (aimat->Get(AI_MATKEY_BASE_COLOR, baseColor) == AI_SUCCESS ||
				aimat->Get(AI_MATKEY_COLOR_DIFFUSE, baseColor) == AI_SUCCESS)
				material->BaseColor = { baseColor.r, baseColor.g, baseColor.b };

			ai_real metallic = 0.0f, roughness = 1.0f;
			bool hasMetallicFactor  = aimat->Get(AI_MATKEY_METALLIC_FACTOR, metallic) == AI_SUCCESS;
			bool hasRoughnessFactor = aimat->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) == AI_SUCCESS;

			// Transparency (glTF alphaMode). Formats without it stay Opaque.
			material->Opacity = baseColor.a;   // baseColorFactor.a
			aiString alphaMode;
			if (aimat->Get(AI_MATKEY_GLTF_ALPHAMODE, alphaMode) == AI_SUCCESS)
			{
				std::string mode = alphaMode.C_Str();
				if (mode == "MASK")       material->Alpha = AlphaMode::Mask;
				else if (mode == "BLEND") material->Alpha = AlphaMode::Blend;
			}
			ai_real alphaCutoff = 0.5f;
			if (aimat->Get(AI_MATKEY_GLTF_ALPHACUTOFF, alphaCutoff) == AI_SUCCESS)
				material->AlphaCutoff = alphaCutoff;

			// DIFFUSE falls back to BASE_COLOR inside ImportTextureSlot, covering both legacy and PBR exports.
			material->BaseColorMap = ImportTextureSlot(aimat, aiTextureType_DIFFUSE, modelDir, assetManager.get(), texCache);
			material->NormalMap    = ImportNormalSlot(aimat, modelDir, assetManager.get(), texCache);
			material->MetallicMap  = ImportTextureSlot(aimat, aiTextureType_METALNESS, modelDir, assetManager.get(), texCache);
			material->RoughnessMap = ImportTextureSlot(aimat, aiTextureType_DIFFUSE_ROUGHNESS, modelDir, assetManager.get(), texCache);
			// glTF occlusion lands under LIGHTMAP in Assimp; other formats use AMBIENT_OCCLUSION.
			material->AOMap        = ImportTextureSlot(aimat, aiTextureType_AMBIENT_OCCLUSION, modelDir, assetManager.get(), texCache);
			if (material->AOMap == 0)
				material->AOMap    = ImportTextureSlot(aimat, aiTextureType_LIGHTMAP, modelDir, assetManager.get(), texCache);

			// Shading is factor × map (white map when absent): with a map and no explicit factor,
			// 1.0 is the neutral multiplier; without either, stay dielectric (0) / fully rough (1).
			material->Metallic  = hasMetallicFactor  ? (float)metallic  : (material->MetallicMap ? 1.0f : 0.0f);
			material->Roughness = hasRoughnessFactor ? (float)roughness : 1.0f;
			// FBX-derived roughness (from shininess) can land outside [0,1].
			material->Metallic  = glm::clamp(material->Metallic, 0.0f, 1.0f);
			material->Roughness = glm::clamp(material->Roughness, 0.0f, 1.0f);

			aiColor3D emissive(0.0f, 0.0f, 0.0f);
			if (aimat->Get(AI_MATKEY_COLOR_EMISSIVE, emissive) == AI_SUCCESS)
				material->Emissive = { emissive.r, emissive.g, emissive.b };
			ai_real emissiveIntensity = 1.0f;
			if (aimat->Get(AI_MATKEY_EMISSIVE_INTENSITY, emissiveIntensity) == AI_SUCCESS)
				material->EmissiveIntensity = emissiveIntensity;
			material->EmissiveMap = ImportTextureSlot(aimat, aiTextureType_EMISSIVE, modelDir, assetManager.get(), texCache);

			aiString aiName;
			aimat->Get(AI_MATKEY_NAME, aiName);
			std::string matName = aiName.length ? aiName.C_Str() : ("mat" + std::to_string(i));

			auto relMatPath = std::filesystem::path("Materials") / (modelStem + "_" + matName + ".tfmat");
			MaterialImporter::Serialize(Project::GetAssetDirectory() / relMatPath, material);
			materialHandles[i] = assetManager->ImportAsset(relMatPath);
		}

		// Walk the node tree, spawning a parented entity hierarchy.
		int entitiesSpawned = 0;
		int meshComponentsAdded = 0;
		int decomposeMismatches = 0;   // node-local transforms that don't survive the Euler round-trip

		std::function<Entity(const aiNode*, Entity)> spawn =
			[&](const aiNode* node, Entity parent) -> Entity
		{
			// Parent at creation (no world-preservation); we set this entity's local transform from
			// the node below, and it inherits the parent's world via the hierarchy.
			const char* name = node->mName.length ? node->mName.C_Str() : "Node";
			Entity entity = parent ? scene->CreateEntity(name, parent) : scene->CreateEntity(name);
			entitiesSpawned++;

			glm::mat4 nodeMat = ToGlm(node->mTransformation);
			glm::vec3 t, r, s;
			Math::DecomposeTransform(nodeMat, t, r, s);
			auto& tc = entity.GetComponent<TransformComponent>();
			tc.Translation = t; tc.Rotation = r; tc.Scale = s;

			// Diagnostic: TransformComponent stores Euler angles, so a node matrix only survives if
			// decompose->recompose round-trips. Non-zero counts here would point back at transforms.
			glm::mat4 recomposed = Math::ComposeTransform(t, r, s);
			float maxErr = 0.0f;
			for (int c = 0; c < 4; c++)
				for (int row = 0; row < 4; row++)
					maxErr = glm::max(maxErr, glm::abs(recomposed[c][row] - nodeMat[c][row]));
			if (maxErr > 1e-3f)
				decomposeMismatches++;

			// Each mesh on this node = one submesh; multiple meshes spawn child entities.
			for (uint32_t m = 0; m < node->mNumMeshes; m++)
			{
				uint32_t submeshIndex = node->mMeshes[m];
				if (submeshIndex >= meshSource->GetSubmeshes().size())
				{
					TF_CORE_WARN("ImportModel - node '{0}' references submesh {1} but only {2} exist; skipping.",
						node->mName.C_Str(), submeshIndex, meshSource->GetSubmeshes().size());
					continue;
				}

				Entity meshEntity = entity;
				if (node->mNumMeshes > 1)
				{
					// Created under the node entity with an identity local transform, so it inherits the
					// node's world transform directly.
					meshEntity = scene->CreateEntity(std::string(node->mName.C_Str()) + "_" + std::to_string(m), entity);
					entitiesSpawned++;
				}
				auto& mc = meshEntity.AddComponent<MeshComponent>();
				mc.Mesh = meshHandle;
				mc.Submesh = submeshIndex;
				uint32_t matIndex = ascene->mMeshes[submeshIndex]->mMaterialIndex;
				mc.Material = (matIndex < materialHandles.size()) ? materialHandles[matIndex] : (AssetHandle)0;
				meshComponentsAdded++;
			}

			for (uint32_t c = 0; c < node->mNumChildren; c++)
				spawn(node->mChildren[c], entity);

			return entity;
		};

		Entity root = spawn(ascene->mRootNode, Entity{});

		TF_CORE_INFO("ImportModel '{0}': spawned {1} entities, {2} mesh components ({3} node transforms failed the Euler round-trip)",
			modelPath.filename().string(), entitiesSpawned, meshComponentsAdded, decomposeMismatches);

		return root;
	}
}
