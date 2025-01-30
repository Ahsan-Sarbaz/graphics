#include "Model.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <GL/glew.h>
#include <execution>
#include <sstream>
#include <stb_image.h>
#include <set>

#include <meshoptimizer.h>
#include <glm/ext/matrix_transform.hpp>

#include "TextureLoader.h"

std::tuple<std::vector<MeshVertex>, std::vector<unsigned int>> Optimize(const float* vertices, const unsigned int* indices, size_t verticesCount, size_t indicesCount) {

	std::vector<unsigned int> remap(indicesCount);
	size_t vertex_count = meshopt_generateVertexRemap(&remap[0], indices, indicesCount, vertices, verticesCount, sizeof(MeshVertex));

	std::vector<unsigned int> optIndices(indicesCount);
	std::vector<MeshVertex> optVertices(vertex_count);
	meshopt_remapIndexBuffer(optIndices.data(), indices, indicesCount, &remap[0]);
	meshopt_remapVertexBuffer(optVertices.data(), &vertices[0], verticesCount, sizeof(MeshVertex), &remap[0]);

	meshopt_optimizeVertexCache(optIndices.data(), optIndices.data(), indicesCount, vertex_count);
	//	meshopt_optimizeOverdraw(optIndices.data(), optIndices.data(), indicesCount, &optVertices[0].position.x, vertex_count, sizeof(MeshVertex), 1.05f);

	return { std::move(optVertices), std::move(optIndices) };
}

void Model::ProcessMesh(aiMesh* mesh, const aiScene* scene, const char* root, const glm::mat4& transform, bool load_textures)
{
	std::vector<MeshVertex> vertices(mesh->mNumVertices);
	{
		MeshVertex v = {
			0, 0, 0, 0,
			0, 0, 0, 0,
			0, 0, 0, 0,
			0, 0,
		};

		std::fill(vertices.begin(), vertices.end(), v);
	}

	if (mesh->HasPositions())
	{
		for (unsigned int i = 0; i < mesh->mNumVertices; ++i)
		{
#ifdef PACK
			vertices[i].x = meshopt_quantizeHalf(mesh->mVertices[i].x);
			vertices[i].y = meshopt_quantizeHalf(mesh->mVertices[i].y);
			vertices[i].z = meshopt_quantizeHalf(mesh->mVertices[i].z);
			vertices[i].w = meshopt_quantizeHalf(1.0f);
#else
			vertices[i].x = mesh->mVertices[i].x;
			vertices[i].y = mesh->mVertices[i].y;
			vertices[i].z = mesh->mVertices[i].z;
			vertices[i].w = 1.0f;
#endif // PACK

		}
	}

	if (mesh->HasNormals()) {
		for (unsigned int i = 0; i < mesh->mNumVertices; ++i)
		{
#ifdef PACK
			vertices[i].nx = uint8_t(mesh->mNormals[i].x * 127.f + 127.5f);
			vertices[i].ny = uint8_t(mesh->mNormals[i].y * 127.f + 127.5f);
			vertices[i].nz = uint8_t(mesh->mNormals[i].z * 127.f + 127.5f);
			vertices[i].nw = 1 * 127.f + 127.5f;
#else
			vertices[i].nx = mesh->mNormals[i].x;
			vertices[i].ny = mesh->mNormals[i].y;
			vertices[i].nz = mesh->mNormals[i].z;
			vertices[i].nw = 1.0f;
#endif

			//vertices[i].normal = { mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z, 0.0f };
		}
	}

	if (mesh->HasTangentsAndBitangents())
	{
		for (unsigned int i = 0; i < mesh->mNumVertices; ++i)
		{
#ifdef PACK

			auto N = glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
			auto T = glm::vec3(mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z);
			auto B = glm::vec3(mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z);

			float handedness = glm::dot(glm::cross(N, T), B) < 0.0f ? -1.0f : 1.0f;

			T = T * handedness;

			vertices[i].tx = uint8_t(T.x * 127.f + 127.5f);
			vertices[i].ty = uint8_t(T.y * 127.f + 127.5f);
			vertices[i].tz = uint8_t(T.z * 127.f + 127.5f);
			vertices[i].tw = uint8_t(handedness * 127.f + 127.5f);

			//vertices[i].bx = uint8_t(B.x * 127.f + 127.5f);
			//vertices[i].by = uint8_t(B.y * 127.f + 127.5f);
			//vertices[i].bz = uint8_t(B.z * 127.f + 127.5f);
			//vertices[i].bw = uint8_t(1 * 127.f + 127.5f);

#else 
			vertices[i].tx = mesh->mTangents[i].x;
			vertices[i].ty = mesh->mTangents[i].y;
			vertices[i].tz = mesh->mTangents[i].z;
			vertices[i].tw = 1.0f;

			//vertices[i].bx = mesh->mBitangents[i].x;
			//vertices[i].by = mesh->mBitangents[i].y;
			//vertices[i].bz = mesh->mBitangents[i].z;
			//vertices[i].bw = 1.0f;
#endif

			//vertices[i].tangent = { mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z, 0.0 };
		}
	}

	if (mesh->HasTextureCoords(0))
	{
		for (unsigned int i = 0; i < mesh->mNumVertices; ++i)
		{
			vertices[i].u = meshopt_quantizeHalf(mesh->mTextureCoords[0][i].x);
			vertices[i].v = meshopt_quantizeHalf(mesh->mTextureCoords[0][i].y);

			//vertices[i].uv = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y };
		}
	}

	std::vector<unsigned int> indices;
	indices.reserve(mesh->mNumFaces * 3);
	for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
		aiFace face = mesh->mFaces[i];
		for (unsigned int j = 0; j < face.mNumIndices; j++)
			indices.push_back(face.mIndices[j]);
	}

#define OPTIMIZE

#ifdef OPTIMIZE
	auto [optVertices, optIndices] = Optimize((const float*)vertices.data(), indices.data(), vertices.size(), indices.size());

	vertices.clear();
	indices.clear();

	auto verticesSize = sizeof(MeshVertex) * optVertices.size();
	auto indicesSize = sizeof(unsigned int) * optIndices.size();

	Mesh gpuMesh = {
		.vertices = std::move(optVertices),
		.indices = std::move(optIndices),
		.textures = {},
		.transform = transform,
		.bounds = {
			.min = {mesh->mAABB.mMin.x, mesh->mAABB.mMin.y, mesh->mAABB.mMin.z},
			.max = {mesh->mAABB.mMax.x, mesh->mAABB.mMax.y, mesh->mAABB.mMax.z},
		},
		.visible = true,
	};

#else
	auto verticesSize = sizeof(MeshVertex) * vertices.size();
	auto indicesSize = sizeof(unsigned int) * indices.size();

	Mesh gpuMesh = {
		.vertices = std::move(vertices),
		.indices = std::move(indices),
		.textures = {},
		.transform = transform,
		.bounds = {
			.min = {mesh->mAABB.mMin.x, mesh->mAABB.mMin.y, mesh->mAABB.mMin.z},
			.max = {mesh->mAABB.mMax.x, mesh->mAABB.mMax.y, mesh->mAABB.mMax.z},
		},
		.visible = true,
	};

#endif // OPTIMIZE


	bool is_metallic_roughness = false;
	bool is_specular_glossiness = false;
	bool is_pbr = false;

	if (scene->HasMaterials())
	{
		auto mat = scene->mMaterials[mesh->mMaterialIndex];

		aiColor4D base_color = { 1.0f, 1.0f, 1.0f, 1.0f };
		auto ret = aiGetMaterialColor(mat, AI_MATKEY_BASE_COLOR, &base_color);
		gpuMesh.base_color = { base_color.r, base_color.g, base_color.b, base_color.a };

		aiColor4D emissive = { 0.0f, 0.0f, 0.0f, 1.0f };
		ret = aiGetMaterialColor(mat, AI_MATKEY_COLOR_EMISSIVE, &emissive);
		gpuMesh.emissive_color = { emissive.r, emissive.g, emissive.b, emissive.a };

		aiColor4D specular = { 0.0f, 0.0f, 0.0f, 1.0f };
		ret = aiGetMaterialColor(mat, AI_MATKEY_COLOR_SPECULAR, &specular);
		gpuMesh.specular_color = { specular.r, specular.g, specular.b, specular.a };


		int shading_model;
		mat->Get(AI_MATKEY_SHADING_MODEL, shading_model);
		float metallic_factor;
		mat->Get(AI_MATKEY_METALLIC_FACTOR, metallic_factor);
		float glossiness_factor;
		mat->Get(AI_MATKEY_GLOSSINESS_FACTOR, glossiness_factor);
		if (shading_model == aiShadingMode_PBR_BRDF)
		{
			is_pbr = true;
			if (metallic_factor > 0) is_metallic_roughness = true;
			if (glossiness_factor > 0) is_specular_glossiness = true;
		}


		if (load_textures) {
			auto mat = scene->mMaterials[mesh->mMaterialIndex];

			// loading base color, roughness, metallic
			if (is_metallic_roughness) {

				if (mat->GetTextureCount(aiTextureType_BASE_COLOR) > 0)
				{
					aiString path;
					mat->GetTexture(aiTextureType_BASE_COLOR, 0, &path);

					if (path.C_Str()[0] == '*')
					{
						auto texture = scene->GetEmbeddedTexture(path.C_Str());
						gpuMesh.textures[BASE_COLOR_MAP_INDEX] = TextureLoader::Load(path.C_Str(), texture->pcData, texture->mWidth, true);
					}
					else {
						std::stringstream ss;
						ss << root << "\\" << path.C_Str();
						gpuMesh.textures[BASE_COLOR_MAP_INDEX] = TextureLoader::Load(ss.str(), nullptr, 0, true);
					}
				}
				else if (mat->GetTextureCount(aiTextureType_DIFFUSE) > 0)
				{
					aiString path;
					mat->GetTexture(aiTextureType_DIFFUSE, 0, &path);

					if (path.C_Str()[0] == '*')
					{
						auto texture = scene->GetEmbeddedTexture(path.C_Str());
						gpuMesh.textures[BASE_COLOR_MAP_INDEX] = TextureLoader::Load(path.C_Str(), texture->pcData, texture->mWidth, true);
					}
					else {
						std::stringstream ss;
						ss << root << "\\" << path.C_Str();
						gpuMesh.textures[BASE_COLOR_MAP_INDEX] = TextureLoader::Load(ss.str(), nullptr, 0, true);
					}
				}


				// this should be the occlusion metallic roughness map
				if (mat->GetTextureCount(aiTextureType_METALNESS) > 0)
				{
					aiString path;
					mat->GetTexture(aiTextureType_METALNESS, 0, &path);

					if (path.C_Str()[0] == '*')
					{
						auto texture = scene->GetEmbeddedTexture(path.C_Str());
						gpuMesh.textures[OCCLUSION_METALLIC_ROUGHNESS_MAP_INDEX] = TextureLoader::Load(path.C_Str(), texture->pcData, texture->mWidth, false);
					}
					else {
						std::stringstream ss;
						ss << root << "\\" << path.C_Str();
						gpuMesh.textures[OCCLUSION_METALLIC_ROUGHNESS_MAP_INDEX] = TextureLoader::Load(ss.str(), nullptr, 0, false);
					}
				}
				else if (mat->GetTextureCount(aiTextureType_DIFFUSE_ROUGHNESS) > 0)
				{
					aiString path;
					mat->GetTexture(aiTextureType_DIFFUSE_ROUGHNESS, 0, &path);

					if (path.C_Str()[0] == '*')
					{
						auto texture = scene->GetEmbeddedTexture(path.C_Str());
						gpuMesh.textures[OCCLUSION_METALLIC_ROUGHNESS_MAP_INDEX] = TextureLoader::Load(path.C_Str(), texture->pcData, texture->mWidth, false);
					}
					else {
						std::stringstream ss;
						ss << root << "\\" << path.C_Str();
						gpuMesh.textures[OCCLUSION_METALLIC_ROUGHNESS_MAP_INDEX] = TextureLoader::Load(ss.str(), nullptr, 0, false);
					}
				}

				if (mat->GetTextureCount(aiTextureType_NORMALS) > 0)
				{
					aiString path;
					mat->GetTexture(aiTextureType_NORMALS, 0, &path);

					if (path.C_Str()[0] == '*')
					{
						auto texture = scene->GetEmbeddedTexture(path.C_Str());
						gpuMesh.textures[NORMAL_MAP_INDEX] = TextureLoader::Load(path.C_Str(), texture->pcData, texture->mWidth, false);
					}
					else {
						std::stringstream ss;
						ss << root << "\\" << path.C_Str();
						gpuMesh.textures[NORMAL_MAP_INDEX] = TextureLoader::Load(ss.str(), nullptr, 0, false);
					}
				}

				if (mat->GetTextureCount(aiTextureType_EMISSIVE) > 0)
				{
					aiString path;
					mat->GetTexture(aiTextureType_EMISSIVE, 0, &path);

					if (path.C_Str()[0] == '*')
					{
						auto texture = scene->GetEmbeddedTexture(path.C_Str());
						gpuMesh.textures[EMISSIVE_MAP_INDEX] = TextureLoader::Load(path.C_Str(), texture->pcData, texture->mWidth, true);
					}
					else {
						std::stringstream ss;
						ss << root << "\\" << path.C_Str();
						gpuMesh.textures[EMISSIVE_MAP_INDEX] = TextureLoader::Load(ss.str(), nullptr, 0, true);
					}
				}
			}
		}
	}


	meshes.push_back(gpuMesh);
}

static inline glm::mat4 AssimpMat4ToGlmMat4(const aiMatrix4x4& from)
{
	glm::mat4 to{};
	to[0][0] = from.a1; to[1][0] = from.a2; to[2][0] = from.a3; to[3][0] = from.a4;
	to[0][1] = from.b1; to[1][1] = from.b2; to[2][1] = from.b3; to[3][1] = from.b4;
	to[0][2] = from.c1; to[1][2] = from.c2; to[2][2] = from.c3; to[3][2] = from.c4;
	to[0][3] = from.d1; to[1][3] = from.d2; to[2][3] = from.d3; to[3][3] = from.d4;
	return to;
}

void Model::ProcessNode(aiNode* node, const aiScene* scene, const char* root, const glm::mat4& transform, bool load_textures)
{
	glm::mat4 local_transform = transform * AssimpMat4ToGlmMat4(node->mTransformation);

	for (unsigned int i = 0; i < node->mNumMeshes; ++i)
	{
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
		ProcessMesh(mesh, scene, root, local_transform, load_textures);
	}

	for (unsigned int i = 0; i < node->mNumChildren; ++i)
	{
		ProcessNode(node->mChildren[i], scene, root, local_transform, load_textures);
	}
}

bool Model::Load(const char* root, const char* filename, float scale, bool load_textures)
{
	Assimp::Importer importer;
	char fullPath[256];

	sprintf_s(fullPath, "%s\\%s", root, filename);

	const aiScene* scene = importer.ReadFile(fullPath,
		aiProcess_Triangulate |
		aiProcess_FlipUVs |
		aiProcess_ForceGenNormals |
		aiProcess_CalcTangentSpace |
		aiProcess_GenBoundingBoxes);
	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
		return false;
	}

	auto transform = AssimpMat4ToGlmMat4(scene->mRootNode->mTransformation);
	transform *= glm::scale(glm::mat4(1.0f), { scale, scale, scale });
	ProcessNode(scene->mRootNode, scene, root, transform, load_textures);

	bounds.min = meshes[0].bounds.min;
	bounds.max = meshes[0].bounds.max;

	for (const auto& mesh : meshes) {
		bounds.min = glm::min(bounds.min, mesh.bounds.min);
		bounds.max = glm::max(bounds.max, mesh.bounds.max);
	}

	if (load_textures) {
		TextureLoader::Get()->LoadPromisedTextures();
	}

	if (scene->HasCameras()) {
		for (unsigned int i = 0; i < scene->mNumCameras; ++i) {
			aiCamera* cam = scene->mCameras[i];

			this->camera_position = glm::vec3(cam->mPosition.x, cam->mPosition.y, cam->mPosition.z);
			this->camera_target = glm::vec3(cam->mLookAt.x, cam->mLookAt.y, cam->mLookAt.z);
			this->camera_up = glm::vec3(cam->mUp.x, cam->mUp.y, cam->mUp.z);
			this->camera_fov = cam->mHorizontalFOV;
			this->camera_far = cam->mClipPlaneFar;
			this->camera_near = cam->mClipPlaneNear;
			this->camera_aspect = cam->mAspect;
		}
	}

	importer.FreeScene();

	return true;
}

void Model::DestroyCpuSideBuffer() {
	for (int i = 0; i < meshes.size(); i++)
	{
		meshes[i].vertices.clear();
		meshes[i].indices.clear();
	}
}


void Model::Destroy()
{
	std::set<ogl::Texture2D*> unique_textures;

	for (int i = 0; i < meshes.size(); i++)
	{
		for (int j = 0; j < 6; j++)
		{
			if (meshes[i].textures[j] != nullptr)
			{
				unique_textures.insert(meshes[i].textures[j]);
			}
		}

		meshes[i].vertices.clear();
		meshes[i].indices.clear();
	}

	for (auto& t : unique_textures)
	{
		ogl::delete_texture(*t);
	}

	meshes.clear();
}