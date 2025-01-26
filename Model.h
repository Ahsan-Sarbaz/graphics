#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <string>

#include "opengl.h"

#define PACK

#ifdef PACK
struct alignas(8) MeshVertex {
	uint16_t x, y, z, w;
	uint8_t nx, ny, nz, nw;
	uint8_t tx, ty, tz, tw;
	//uint8_t bx, by, bz, bw;
	uint16_t u, v;
};

#else
struct alignas(16) MeshVertex {
	float x, y, z, w;
	float nx, ny, nz, nw;
	float tx, ty, tz, tw;
	//float bx, by, bz, bw;
	float u, v, _p1, _p2;
};
#endif

#define BASE_COLOR_MAP_INDEX 0
#define DIFFUSE_MAP_INDEX 0

#define NORMAL_MAP_INDEX 1
#define NORMAL_CAMERA_MAP_INDEX 1

#define SHININESS_MAP_INDEX 2
#define DIFFUSE_ROUGHNESS_MAP_INDEX 2

#define EMISSIVE_MAP_INDEX 3
#define EMISSIVE_COLOR_MAP_INDEX 3

#define SPECULAR_MAP_INDEX 4
#define METALNESS_MAP_INDEX 4

#define AMBIENT_OCCLUSION_MAP_INDEX 5
#define LIGHTMAP_MAP_INDEX 5

struct AABB {
	glm::vec3 min;
	glm::vec3 max;
};

struct Mesh
{
	std::vector<MeshVertex> vertices;
	std::vector<uint32_t> indices;
	ogl::Texture2D* textures[6];
	glm::mat4 transform;
	glm::vec4 base_color;
	glm::vec4 emissive_color;
	glm::vec4 specular_color;
	AABB bounds;
	bool visible;
};

struct aiMesh;
struct aiScene;
struct aiNode;

struct Model
{
	std::vector<Mesh> meshes;
	AABB bounds;
	glm::vec3 camera_position { 0.0f, 0.0f, 0.0f };
	glm::vec3 camera_target { 0.0f, 0.0f, 0.0f };
	glm::vec3 camera_up { 0.0f, 1.0f, 0.0f };
	float camera_fov = 45.0f;
	float camera_aspect = 16.0f / 9.0f;
	float camera_near = 0.1f;
	float camera_far = 1000.0f;


	bool Load(const char* root, const char* filename, float scale = 1.0f, bool load_textures = false);
	void DestroyCpuSideBuffer();

	void Destroy();

	void ProcessMesh(aiMesh* mesh, const aiScene* scene, const char* root, const glm::mat4& transform, bool load_textures);
	void ProcessNode(aiNode* node, const aiScene* scene, const char* root, const glm::mat4& transform, bool load_textures);
};