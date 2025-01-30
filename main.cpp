#define _CRT_SECURE_NO_WARNINGS
#include <cmath>
#include <cstdint>
#include <fstream>
#include <filesystem>
#include <glm/fwd.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/matrix.hpp>
#include <iostream>
#include <vector>
#include <unordered_map>

#include "opengl.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/simd/matrix.h>

#include "stb_image.h"

#include "Model.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <meshoptimizer.h>

#define WINDOW_WIDTH 2160
#define	WINDOW_HEIGHT 1440

GLFWwindow* create_window() {

	if (!glfwInit()) {
		std::cerr << "Failed to initialize GLFW" << std::endl;
		return nullptr;
	}

	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
	//glfwWindowHint(GLFW_SAMPLES, 4);
	GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "CGL", NULL, NULL);
	if (window == nullptr) {
		std::cerr << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return nullptr;
	}
	return window;
}

std::vector<char> read_file(const std::string& filename) {
	auto file = fopen(filename.c_str(), "r");

	fseek(file, 0, SEEK_END);
	auto size = ftell(file);
	fseek(file, 0, SEEK_SET);
	std::vector<char> buffer(size);
	fread(buffer.data(), 1, size, file);
	fclose(file);
	return buffer;
}

struct alignas(16) Vertex {
	glm::vec4 position;
	glm::vec4 normal;
	glm::vec2 uv;
	// float padding[2];
};

#define M_PIf 3.14159265358979323846

// https://www.songho.ca/opengl/gl_sphere.html
void create_sphere(std::vector<MeshVertex>& vertices,
	std::vector<uint16_t>& indices, float radius, int stackCount,
	int sectorCount) {

	float x, y, z, xy;
	float nx, ny, nz, lengthInv = 1.0f / radius;
	float tx, ty, tz;
	float s, t;

	float sectorStep = 2 * float(M_PIf) / sectorCount;
	float stackStep = float(M_PIf) / stackCount;
	float sectorAngle, stackAngle;

	for (int i = 0; i <= stackCount; ++i) {
		stackAngle = float(M_PIf) / 2 - i * stackStep;
		xy = radius * cosf(stackAngle);
		z = radius * sinf(stackAngle);

		for (int j = 0; j <= sectorCount; ++j) {
			sectorAngle = j * sectorStep;

			x = xy * cosf(sectorAngle);
			y = xy * sinf(sectorAngle);

			nx = x * lengthInv;
			ny = y * lengthInv;
			nz = z * lengthInv;

			s = (float)j / sectorCount;
			t = (float)i / stackCount;

			tx = 0;
			ty = 0;
			tz = 0;

			MeshVertex vertex = {
				.x = meshopt_quantizeHalf(x),
				.y = meshopt_quantizeHalf(y),
				.z = meshopt_quantizeHalf(z),
				.w = meshopt_quantizeHalf(1),
				.nx = uint8_t(nx * 127.f + 127.5f),
				.ny = uint8_t(ny * 127.f + 127.5f),
				.nz = uint8_t(nz * 127.f + 127.5f),
				.nw = uint8_t(1 * 127.f + 127.5f),
				.tx = uint8_t(tx * 127.f + 127.5f),
				.ty = uint8_t(ty * 127.f + 127.5f),
				.tz = uint8_t(tz * 127.f + 127.5f),
				.tw = uint8_t(1 * 127.f + 127.5f),
				.u = meshopt_quantizeHalf(s),
				.v = meshopt_quantizeHalf(t)
			};
			vertices.push_back(vertex);
		}
	}

	std::vector<int> lineIndices;
	int k1, k2;
	for (int i = 0; i < stackCount; ++i) {
		k1 = i * (sectorCount + 1);
		k2 = k1 + sectorCount + 1;

		for (int j = 0; j < sectorCount; ++j, ++k1, ++k2) {
			if (i != 0) {
				indices.push_back(k1);
				indices.push_back(k2);
				indices.push_back(k1 + 1);
			}

			if (i != (stackCount - 1)) {
				indices.push_back(k1 + 1);
				indices.push_back(k2);
				indices.push_back(k2 + 1);
			}

			lineIndices.push_back(k1);
			lineIndices.push_back(k2);
			if (i != 0) {
				lineIndices.push_back(k1);
				lineIndices.push_back(k1 + 1);
			}
		}
	}
}

struct Image {
	uint16_t* data;
	int width;
	int height;
	int channels;
};

Image load_image(const std::string& filename) {
	Image image;
	image.data = (uint16_t*)stbi_load(filename.c_str(), &image.width,
		&image.height, &image.channels, 0);
	if (image.data == nullptr) {
		std::cerr << "Failed to load image: " << filename << std::endl;
		return {};
	}
	return image;
}

void free_image(Image image) {
	if (image.data == nullptr) {
		return;
	}
	stbi_image_free(image.data);
}

struct GPUObject {
	ogl::Buffer vertex_buffer;
	ogl::Buffer index_buffer;
	ogl::Texture2D* textures[4];
	glm::mat4 transform;
	glm::vec4 base_color;
	glm::vec4 emissive_color;
	glm::vec4 specular_color;
	AABB bounds;
	uint32_t indices_count;
	bool index_buffer_short;
	bool visible;
};


std::vector<GPUObject> load_model(Model& model) {

	std::vector<GPUObject> gpu_objects(model.meshes.size());

	for (int i = 0; i < model.meshes.size(); i++)
	{

		GPUObject gpu_object;

		gpu_object.vertex_buffer = ogl::create_buffer(model.meshes[i].vertices.data(), model.meshes[i].vertices.size() * sizeof(MeshVertex), false);
		if (model.meshes[i].indices.size() > std::numeric_limits<uint16_t>::max())
		{
			gpu_object.index_buffer = ogl::create_buffer(model.meshes[i].indices.data(), model.meshes[i].indices.size() * sizeof(uint32_t), false);
			gpu_object.index_buffer_short = false;
		}
		else {
			std::vector<uint16_t> indices16(model.meshes[i].indices.size());
			for (int j = 0; j < model.meshes[i].indices.size(); j++)
			{
				indices16[j] = model.meshes[i].indices[j];
			}
			gpu_object.index_buffer = ogl::create_buffer(indices16.data(), indices16.size() * sizeof(uint16_t), false);
			gpu_object.index_buffer_short = true;
		}

		gpu_object.indices_count = uint32_t(model.meshes[i].indices.size());

		for (int j = 0; j < _countof(model.meshes[i].textures); j++)
		{
			if (model.meshes[i].textures[j] != nullptr)
			{
				gpu_object.textures[j] = model.meshes[i].textures[j];
			}
			else {
				gpu_object.textures[j] = nullptr;
			}
		}

		gpu_object.transform = model.meshes[i].transform;
		gpu_object.base_color = model.meshes[i].base_color;
		gpu_object.emissive_color = model.meshes[i].emissive_color;
		gpu_object.specular_color = model.meshes[i].specular_color;
		gpu_object.bounds = model.meshes[i].bounds;
		gpu_object.visible = true;

		gpu_objects[i] = gpu_object;
	}

	return gpu_objects;
}


void mouseCallback(GLFWwindow* window, int button, int action, int mods)
{
	if (action == GLFW_PRESS && button == GLFW_MOUSE_BUTTON_RIGHT)
	{
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		glfwSetCursorPos(window, 0, 0);
	}
	else if (action == GLFW_RELEASE && button == GLFW_MOUSE_BUTTON_RIGHT)
	{
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	}
}

struct alignas(16) DirectionalLight {
	glm::vec3 direction;
	float intensity;
	glm::vec3 color;
	float padding;
};

struct alignas(16) PointLight {
	glm::vec3 position;
	float range;
	glm::vec3 color;
	float intensity;
};

struct alignas(16) SpotLight {
	glm::vec3 position;
	float range;
	glm::vec3 direction;
	float intensity;
	glm::vec3 color;
	float angle;
};

struct alignas(16) PerFrame {
	glm::mat4 view;
	glm::mat4 projection;
	glm::vec3 camera_position;
	float padding;
};

struct alignas(16) PerObject {
	glm::mat4 model;
	glm::mat4 normal_matrix;
	glm::vec4 base_color;
	glm::vec4 emissive_color;
	glm::vec4 specular_color;
};

struct RendererState {
	PerFrame per_frame;
	PerObject per_object;

	ogl::Buffer per_frame_buffer;
	ogl::Buffer per_object_buffer;
	ogl::Program fullscreen_quad_program;

	int exposure_location;
	float exposure = 1.0f;
};

RendererState* g_renderer_state;

struct Primitives {
	ogl::Program program;
	ogl::Buffer sphere_vertex_buffer;
	ogl::Buffer sphere_index_buffer;
	int sphere_index_count;
};

Primitives g_primitives;

void init_primitives() {

	auto vertex_shader_source = read_file("primitives_vertex.glsl");
	auto fragment_shader_source = read_file("primitives_fragment.glsl");

	auto vs = ogl::create_shader(GL_VERTEX_SHADER, vertex_shader_source.data());
	auto fs = ogl::create_shader(GL_FRAGMENT_SHADER, fragment_shader_source.data());
	g_primitives.program = ogl::create_program({ vs, fs });

	{
		std::vector<MeshVertex> vertices{};
		std::vector<uint16_t> indices{};

		create_sphere(vertices, indices, 1.0f, 12, 12);

		ogl::Buffer vertex_buffer = ogl::create_buffer(vertices.data(), vertices.size() * sizeof(Vertex));
		ogl::Buffer index_buffer = ogl::create_buffer(indices.data(), indices.size() * sizeof(uint16_t));

		g_primitives.sphere_vertex_buffer = vertex_buffer;
		g_primitives.sphere_index_buffer = index_buffer;
		g_primitives.sphere_index_count = uint32_t(indices.size());
	}
}

void draw_sphere(glm::vec3 position, glm::vec3 scale, glm::vec3 color) {
	ogl::bind_buffer_as_ssbo(g_primitives.sphere_vertex_buffer, 0);
	ogl::bind_buffer_as_ebo(g_primitives.sphere_index_buffer);

	glm::mat4 model = glm::mat4(1.0f);
	model = glm::translate(model, position);
	model = glm::scale(model, scale);

	g_renderer_state->per_object.base_color = glm::vec4(color, 1.0f);
	g_renderer_state->per_object.model = model;
	g_renderer_state->per_object.normal_matrix = glm::mat3(glm::transpose(glm::inverse(model)));

	ogl::buffer_subdata(g_renderer_state->per_object_buffer, &g_renderer_state->per_object, sizeof(PerObject), 0);

	ogl::use_program(g_primitives.program);
	glDrawElements(GL_TRIANGLES, g_primitives.sphere_index_count, GL_UNSIGNED_SHORT, 0);
}

void EnableDebugMessages();

void fullscreen_quad_shader()
{

	const char* v_source = R"(
	#version 460

	out vec2 out_tex_coords;

	void main() {
		vec4 positions[4] = {
			vec4(-1.0, -1.0, 0.0, 1.0),
			vec4(1.0, -1.0, 0.0, 1.0),
			vec4(1.0, 1.0, 0.0, 1.0),
			vec4(-1.0, 1.0, 0.0, 1.0)
		};

		vec2 tex_coords[4] = {
			vec2(0.0, 0.0),
			vec2(1.0, 0.0),
			vec2(1.0, 1.0),
			vec2(0.0, 1.0)
		};

		gl_Position = positions[gl_VertexID];
		out_tex_coords = tex_coords[gl_VertexID];
	}
	)";

	const char* f_source = R"(
	#version 460
	in vec2 out_tex_coords;

	out vec4 color;

	uniform sampler2D tex;
	uniform float exposure;

	void main() {
		const float gamma = 2.2;
		vec3 hdrColor = texture(tex, out_tex_coords).rgb;
  
		// reinhard tone mapping
		vec3 mapped = vec3(1.0) - exp(-hdrColor * exposure);
		// gamma correction 
		mapped = pow(mapped, vec3(1.0 / gamma));
		color = vec4(mapped, 1.0);
	}
	)";


	auto vs = ogl::create_shader(GL_VERTEX_SHADER, v_source);
	auto fs = ogl::create_shader(GL_FRAGMENT_SHADER, f_source);

	g_renderer_state->fullscreen_quad_program = ogl::create_program({ vs, fs });
	g_renderer_state->exposure_location = glGetUniformLocation(g_renderer_state->fullscreen_quad_program.id, "exposure");
}

void draw_fullscreen_quad(ogl::Texture2D texture) {
	ogl::use_program(g_renderer_state->fullscreen_quad_program);
	glUniform1f(g_renderer_state->exposure_location, g_renderer_state->exposure);
	ogl::bind_texture(texture, 0);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}


struct Shader {
	std::string name;
	std::string vs_path;
	std::string fs_path;
	ogl::Program program;
	std::filesystem::file_time_type vertex_glsl_last_modified, fragment_glsl_last_modified;
};

void watch_and_reload_program(Shader& shader) {
	auto vertex_glsl_last_modified_new = std::filesystem::last_write_time(shader.vs_path);
	auto fragment_glsl_last_modified_new = std::filesystem::last_write_time(shader.fs_path);

	if (vertex_glsl_last_modified_new != shader.vertex_glsl_last_modified || fragment_glsl_last_modified_new != shader.fragment_glsl_last_modified) {
		shader.vertex_glsl_last_modified = vertex_glsl_last_modified_new;

		auto vertex_shader_source = read_file(shader.vs_path);
		auto vertex_shader_source_ptr = vertex_shader_source.data();
		auto vertex_shader = ogl::create_shader(GL_VERTEX_SHADER, vertex_shader_source_ptr);

		shader.fragment_glsl_last_modified = fragment_glsl_last_modified_new;

		auto fragment_shader_source = read_file(shader.fs_path);
		auto fragment_shader_source_ptr = fragment_shader_source.data();
		auto fragment_shader = ogl::create_shader(GL_FRAGMENT_SHADER, fragment_shader_source_ptr);

		if (vertex_shader.id != 0 && fragment_shader.id != 0) {
			std::cout << "Reloading shader " << shader.name << std::endl;
			shader.program = ogl::create_program({ vertex_shader, fragment_shader });
		}
		else {
			std::cerr << "Failed to create program" << std::endl;
		}
	}
}

struct ShaderLoader {
	std::unordered_map<std::string, Shader> programs{};
};

ShaderLoader g_shader_loader;

void load_shader(const std::string& name, const char* vs_path, const char* fs_path) {

	auto vs_source = read_file(vs_path);
	auto fs_source = read_file(fs_path);

	auto vs_source_ptr = vs_source.data();
	auto fs_source_ptr = fs_source.data();

	auto vs = ogl::create_shader(GL_VERTEX_SHADER, vs_source_ptr);
	auto fs = ogl::create_shader(GL_FRAGMENT_SHADER, fs_source_ptr);

	auto program = ogl::create_program({ vs, fs });

	auto vs_last_modified = std::filesystem::last_write_time(vs_path);
	auto fs_last_modified = std::filesystem::last_write_time(fs_path);

	g_shader_loader.programs[name] = Shader{
		.name = name,
		.vs_path = vs_path,
		.fs_path = fs_path,
		.program = program,
		.vertex_glsl_last_modified = vs_last_modified,
		.fragment_glsl_last_modified = fs_last_modified
	};
}

void reload_shaders() {
	for (auto& [name, shader] : g_shader_loader.programs) {
		watch_and_reload_program(shader);
	}
}

void use_shader(const std::string& name) {
	const auto& shader = g_shader_loader.programs[name];
	ogl::use_program(shader.program);
}

int main(int argc, char* argv[]) {
	(void)argc;
	(void)argv;

	GLFWwindow* window = create_window();
	if (window == nullptr) {
		return -1;
	}

	glfwSetMouseButtonCallback(window, mouseCallback);

	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);

	if (!ogl::init()) {
		return -1;
	}

	EnableDebugMessages();

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	//ImGui::StyleColorsLight();

	// Setup Platform/Renderer backends
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 460");


	double last_time = glfwGetTime();
	double current_time = 0.0;
	double delta_time = 0.0;

	// set clear color sky blue
	//   glClearColor(135.0f / 255.0f, 206.0f / 255.0f, 235.0f / 255.0f, 1.0f);

	g_renderer_state = new RendererState();

	init_primitives();

	{
		ogl::Buffer per_frame_buffer = ogl::create_buffer(nullptr, sizeof(PerFrame), true);
		ogl::Buffer per_object_buffer = ogl::create_buffer(nullptr, sizeof(PerObject), true);

		g_renderer_state->per_frame_buffer = per_frame_buffer;
		g_renderer_state->per_object_buffer = per_object_buffer;


		ogl::bind_buffer_as_ubo(per_frame_buffer, 0);
		ogl::bind_buffer_as_ubo(per_object_buffer, 1);
	}

	ogl::Buffer directional_light_buffer = ogl::create_buffer(nullptr, sizeof(DirectionalLight), true);
	ogl::Buffer points_light_buffer = ogl::create_buffer(nullptr, sizeof(PointLight) * 4, true);
	ogl::bind_buffer_as_ubo(directional_light_buffer, 2);
	ogl::bind_buffer_as_ubo(points_light_buffer, 3);



	auto vao = ogl::create_vertex_array();
	ogl::bind_vertex_array(vao);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glFrontFace(GL_CCW);
	glCullFace(GL_BACK);


	Model model;
	//std::filesystem::path p = "C:\\dev\\pbr\\pbr\\\models\\WaterBottle.gltf";
	std::filesystem::path p = "C:\\dev\\pbr\\pbr\\niagara_bistro\\bistro.gltf";
	//std::filesystem::path p = "models/Sponza.glb";
	//std::filesystem::path p = "models/FlightHelmet.gltf";
	//std::filesystem::path p = "models/DamagedHelmet.glb";
	//std::filesystem::path p = "models/Corset.glb";


	auto root = p.parent_path().string();
	auto filename_str = p.filename().string();

	model.Load(root.c_str(), filename_str.c_str(), 1.0f, true);

	auto gpu_objects = load_model(model);

	model.DestroyCpuSideBuffer();

	//glm::vec3 camera_position = glm::vec3(-24.0f, 4.6f, 13.0f);
	//glm::quat camera_orientation =glm::quat(0.782239f, -0.0456291f, -0.62025f, -0.0361797f);
	glm::vec3 camera_position = glm::vec3(-0.572695f, 0.181061f, -0.00497043f);
	glm::quat camera_orientation = glm::quat(-0.741468f, -0.0250291f, 0.670137f, -0.0226173f);
	//glm::vec3 camera_position = glm::vec3(-0.201776f, 5.62013f, 9.00883f);
	//glm::quat camera_orientation =glm::quat(-0.993706f, 0.111626f, -0.00852995f, -0.000959459f);

	std::cout << "camera_position: " << camera_position.x << ", " << camera_position.y << ", " << camera_position.z << std::endl;

	glm::vec3 sun_direction = glm::vec3(0.0f, -1.0f, 0.0f);


	double last_mouse_x = 0.0;
	double last_mouse_y = 0.0;

	uint64_t frames = 0;

	//	glEnable(GL_MULTISAMPLE);

	DirectionalLight sun = {
		.direction = glm::normalize(sun_direction),
		.intensity = 10.0f,
		.color = glm::vec3(1.0f),
		.padding = 0.0f,
	};

	PointLight point_lights[4] = {
		{
			.position = glm::vec3(1.0f, 0.0f, 1.0f),
			.range = 1.0f,
			.color = glm::vec3(1.0f, 0.0f, 0.0f),
			.intensity = 5.0f,
		},
		{
			.position = glm::vec3(1.0f, 0.0f, -1.0f),
			.range = 1.0f,
			.color = glm::vec3(0.0f, 1.0f, 0.0f),
			.intensity = 5.0f,
		},
		{
			.position = glm::vec3(-1.0f, 0.0f, 1.0f),
			.range = 1.0f,
			.color = glm::vec3(0.0f, 0.0f, 1.0f),
			.intensity = 5.0f,
		},
		{
			.position = glm::vec3(-1.0f, 0.0f, -1.0f),
			.range = 1.0f,
			.color = glm::vec3(1.0f, 1.0f, 0.0f),
			.intensity = 5.0f,
		},
	};

	auto hdr_framebuffer = ogl::create_framebuffer(WINDOW_WIDTH, WINDOW_HEIGHT);
	{
		auto color_attachment0 = ogl::create_framebuffer_attachment(hdr_framebuffer, GL_RGB16F, true);

		ogl::framebuffer_color_attachment(hdr_framebuffer, color_attachment0, 0);
		ogl::framebuffer_draw_attachments(hdr_framebuffer);
	}

	auto hdr_forward_framebuffer = ogl::create_framebuffer(WINDOW_WIDTH, WINDOW_HEIGHT);
	{
		auto color_attachment0 = ogl::create_framebuffer_attachment(hdr_forward_framebuffer, GL_RGB16F, true);
		auto depth_attachment = ogl::create_framebuffer_attachment(hdr_forward_framebuffer, GL_DEPTH_COMPONENT32F, false);

		ogl::framebuffer_color_attachment(hdr_forward_framebuffer, color_attachment0, 0);
		ogl::framebuffer_depth_attachment(hdr_forward_framebuffer, depth_attachment);
		ogl::framebuffer_draw_attachments(hdr_forward_framebuffer);
	}


	fullscreen_quad_shader();

	auto gbuffer_framebuffer = ogl::create_framebuffer(WINDOW_WIDTH, WINDOW_HEIGHT);
	{
		auto color_attachment0 = ogl::create_framebuffer_attachment(gbuffer_framebuffer, GL_RGBA16F, true); // positions
		auto color_attachment1 = ogl::create_framebuffer_attachment(gbuffer_framebuffer, GL_SRGB8_ALPHA8, true); // albedo
		auto color_attachment2 = ogl::create_framebuffer_attachment(gbuffer_framebuffer, GL_RGB10_A2, true); // normals
		auto color_attachment3 = ogl::create_framebuffer_attachment(gbuffer_framebuffer, GL_RGB10_A2, true); // view position
		auto color_attachment4 = ogl::create_framebuffer_attachment(gbuffer_framebuffer, GL_RGBA8, true); // orm
		auto depth_attachment = ogl::create_framebuffer_attachment(gbuffer_framebuffer, GL_DEPTH_COMPONENT32F, false); // depth

		ogl::framebuffer_color_attachment(gbuffer_framebuffer, color_attachment0, 0);
		ogl::framebuffer_color_attachment(gbuffer_framebuffer, color_attachment1, 1);
		ogl::framebuffer_color_attachment(gbuffer_framebuffer, color_attachment2, 2);
		ogl::framebuffer_color_attachment(gbuffer_framebuffer, color_attachment3, 3);
		ogl::framebuffer_color_attachment(gbuffer_framebuffer, color_attachment4, 4);
		ogl::framebuffer_depth_attachment(gbuffer_framebuffer, depth_attachment);

		ogl::framebuffer_draw_attachments(gbuffer_framebuffer);
	}

	load_shader("deferred", "deferred_vertex.glsl", "deferred_fragment.glsl");
	load_shader("deferred_lighting", "deferred_lighting_vertex.glsl", "deferred_lighting_fragment.glsl");
	load_shader("forward", "vertex.glsl", "fragment.glsl");

	float exposure = 1.0f;

	bool deferred = false;

	uint32_t black_pixel = 0xFF000000;
	ogl::Texture2D black_texture = ogl::create_texture_from_bytes(&black_pixel, 1, 1, 1, 4, false);

	uint32_t white_pixel = 0xFFFFFFFF;
	ogl::Texture2D white_texture = ogl::create_texture_from_bytes(&white_pixel, 1, 1, 1, 4, false);

	while (!glfwWindowShouldClose(window)) {
		frames++;

		if (frames % 60 == 0) {
			reload_shaders();
		}

		glfwPollEvents();

		current_time = glfwGetTime();
		delta_time = current_time - last_time;
		last_time = current_time;

		if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
			glfwSetWindowShouldClose(window, true);
		}

		if (glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS) {
			std::cout << "camera_position: " << camera_position.x << ", " << camera_position.y << ", " << camera_position.z << std::endl;
			std::cout << "camera_orientation: " << camera_orientation.w << ", " << camera_orientation.x << ", " << camera_orientation.y << ", " << camera_orientation.z << std::endl;
		}

		if (glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED)
		{
			double xpos, ypos;
			glfwGetCursorPos(window, &xpos, &ypos);

			bool cameraBoost = (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS);
			glm::vec2 cameraMotion = glm::vec2(glfwGetKey(window, GLFW_KEY_W), glfwGetKey(window, GLFW_KEY_D)) - glm::vec2(glfwGetKey(window, GLFW_KEY_S), glfwGetKey(window, GLFW_KEY_A));
			glm::vec2 cameraRotation = glm::vec2(xpos, ypos);

			float cameraMotionSpeed = cameraBoost ? 2.f : 0.5f;
			float cameraRotationSpeed = glm::radians(10.f);

			camera_position += float(cameraMotion.y * delta_time * cameraMotionSpeed) * (camera_orientation * glm::vec3(1, 0, 0));
			camera_position += float(cameraMotion.x * delta_time * cameraMotionSpeed) * (camera_orientation * glm::vec3(0, 0, -1));
			camera_orientation = glm::rotate(glm::quat(0, 0, 0, 1), float(-cameraRotation.x * delta_time * cameraRotationSpeed), glm::vec3(0, 1, 0)) * camera_orientation;
			camera_orientation = glm::rotate(glm::quat(0, 0, 0, 1), float(-cameraRotation.y * delta_time * cameraRotationSpeed), camera_orientation * glm::vec3(1, 0, 0)) * camera_orientation;

			glfwSetCursorPos(window, 0, 0);
		}


		int width, height;
		glfwGetFramebufferSize(window, &width, &height);

		if (deferred)
		{
			ogl::bind_framebuffer(gbuffer_framebuffer);

			if (width != gbuffer_framebuffer.width || height != gbuffer_framebuffer.height) {
				ogl::framebuffer_resize(gbuffer_framebuffer, width, height);
			}

			float clear_color[] = { 0.0f, 0.0f, 0.0f, 0.0f };
			glClearNamedFramebufferfv(gbuffer_framebuffer.id, GL_COLOR, 0, clear_color);
			glClearNamedFramebufferfv(gbuffer_framebuffer.id, GL_COLOR, 1, clear_color);
			glClearNamedFramebufferfv(gbuffer_framebuffer.id, GL_COLOR, 2, clear_color);
			glClearNamedFramebufferfv(gbuffer_framebuffer.id, GL_COLOR, 3, clear_color);
			glClearNamedFramebufferfv(gbuffer_framebuffer.id, GL_COLOR, 4, clear_color);

			glClearNamedFramebufferfi(gbuffer_framebuffer.id, GL_DEPTH_STENCIL, 0, 1.0f, 0);
		}
		else {
			ogl::bind_framebuffer(hdr_forward_framebuffer);
			if (width != hdr_forward_framebuffer.width || height != hdr_forward_framebuffer.height) {
				ogl::framebuffer_resize(hdr_forward_framebuffer, width, height);
			}
			float clear_color[] = { 0.0f, 0.0f, 0.0f, 1.0f };
			glClearNamedFramebufferfv(hdr_forward_framebuffer.id, GL_COLOR, 0, clear_color);
			glClearNamedFramebufferfi(hdr_forward_framebuffer.id, GL_DEPTH_STENCIL, 0, 1.0f, 0);
		}

		glViewport(0, 0, width, height);


		g_renderer_state->per_frame.projection = glm::perspective(model.camera_fov, (float)width / (float)height, 0.01f, 10000.0f);

		{

			glm::mat4 view = glm::mat4_cast(camera_orientation);
			view[3] = glm::vec4(camera_position, 1.0f);
			view = inverse(view);
			//view = glm::scale(glm::identity<glm::mat4>(), glm::vec3(1, 1, -1)) * view;

			g_renderer_state->per_frame.view = view;
			g_renderer_state->per_frame.camera_position = glm::vec4(camera_position, 1.0f);
		}

		ogl::buffer_subdata(g_renderer_state->per_frame_buffer, &g_renderer_state->per_frame, sizeof(PerFrame), 0);

		sun.direction = glm::normalize(sun_direction);
		ogl::buffer_subdata(directional_light_buffer, &sun, sizeof(DirectionalLight), 0);
		ogl::buffer_subdata(points_light_buffer, &point_lights, sizeof(point_lights), 0);


		if (deferred) {
			use_shader("deferred");
		}
		else {
			for (auto& point_light : point_lights) {
				draw_sphere(point_light.position, glm::vec3(0.1f), point_light.color);
			}

			use_shader("forward");
		}

		for (const auto& mesh : gpu_objects) {
			ogl::bind_buffer_as_ssbo(mesh.vertex_buffer, 0);
			ogl::bind_buffer_as_ebo(mesh.index_buffer);

			if (mesh.textures[BASE_COLOR_MAP_INDEX] != nullptr && mesh.textures[BASE_COLOR_MAP_INDEX]->id != 0) {
				ogl::bind_texture(*mesh.textures[BASE_COLOR_MAP_INDEX], BASE_COLOR_MAP_INDEX);
			}
			else {
				ogl::bind_texture(black_texture, BASE_COLOR_MAP_INDEX);
			}

			if (mesh.textures[OCCLUSION_METALLIC_ROUGHNESS_MAP_INDEX] != nullptr && mesh.textures[OCCLUSION_METALLIC_ROUGHNESS_MAP_INDEX]->id != 0) {
				ogl::bind_texture(*mesh.textures[OCCLUSION_METALLIC_ROUGHNESS_MAP_INDEX], OCCLUSION_METALLIC_ROUGHNESS_MAP_INDEX);
			}
			else {
				ogl::bind_texture(black_texture, OCCLUSION_METALLIC_ROUGHNESS_MAP_INDEX);
			}

			if (mesh.textures[NORMAL_MAP_INDEX] != nullptr && mesh.textures[NORMAL_MAP_INDEX]->id != 0) {
				ogl::bind_texture(*mesh.textures[NORMAL_MAP_INDEX], NORMAL_MAP_INDEX);
			}
			else {
				ogl::bind_texture(white_texture, NORMAL_MAP_INDEX);
			}


			if (mesh.textures[EMISSIVE_MAP_INDEX] != nullptr && mesh.textures[EMISSIVE_MAP_INDEX]->id != 0) {
				ogl::bind_texture(*mesh.textures[EMISSIVE_MAP_INDEX], EMISSIVE_MAP_INDEX);
			}
			else {
				ogl::bind_texture(black_texture, EMISSIVE_MAP_INDEX);
			}

			g_renderer_state->per_object.model = mesh.transform;
			g_renderer_state->per_object.normal_matrix = glm::transpose(glm::inverse(mesh.transform));
			g_renderer_state->per_object.base_color = mesh.base_color;
			g_renderer_state->per_object.emissive_color = mesh.emissive_color;
			g_renderer_state->per_object.specular_color = mesh.specular_color;

			ogl::buffer_subdata(g_renderer_state->per_object_buffer, &g_renderer_state->per_object, sizeof(PerObject), 0);

			if (mesh.index_buffer_short) {
				glDrawElements(GL_TRIANGLES, mesh.indices_count, GL_UNSIGNED_SHORT, nullptr);
			}
			else {
				glDrawElements(GL_TRIANGLES, mesh.indices_count, GL_UNSIGNED_INT, nullptr);
			}
		}

		if (deferred) {
			ogl::bind_framebuffer(hdr_framebuffer);

			if (hdr_framebuffer.width != width || hdr_framebuffer.height != height) {
				framebuffer_resize(hdr_framebuffer, width, height);
			}

			glViewport(0, 0, width, height);
			glClear(GL_COLOR_BUFFER_BIT);

			use_shader("deferred_lighting");


			for (const auto& attacment : gbuffer_framebuffer.color_attachments) {
				ogl::bind_texture(ogl::Texture2D(attacment.id), attacment.index);
			}

			glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
		}

		ogl::bind_default_framebuffer();
		glViewport(0, 0, width, height);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		if (deferred) {
			draw_fullscreen_quad(ogl::Texture2D{ hdr_framebuffer.color_attachments[0].id });
		}
		else {
			draw_fullscreen_quad(ogl::Texture2D{ hdr_forward_framebuffer.color_attachments[0].id });
		}

		// --------------- ImGui ----------------------- //

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		ImGui::Begin("Settings");

		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);


		ImGui::Checkbox("Deferred", &deferred);
		ImGui::DragFloat3("Sun Direction", glm::value_ptr(sun_direction), 0.01f, -1.0f, 1.0f);
		ImGui::DragFloat("Sun Intensity", &sun.intensity, 0.1f, 0.0f, 10.0f);
		ImGui::ColorEdit3("Sun Color", glm::value_ptr(sun.color));

		ImGui::DragFloat("Exposure", &g_renderer_state->exposure, 0.1f, 0.0f, 10.0f);

		ImGui::Separator();

		for (uint32_t i = 0; i < _countof(point_lights); i++) {
			ImGui::PushID(i);
			char name[32];
			sprintf(name, "Point Light %d", i);
			ImGui::Text(name);
			ImGui::DragFloat3("Position", glm::value_ptr(point_lights[i].position), 0.01f, -3.0f, 3.0f);
			ImGui::DragFloat("Intensity", &point_lights[i].intensity, 0.1f, 0.0f, 10.0f);
			ImGui::ColorEdit3("Color", glm::value_ptr(point_lights[i].color));
			ImGui::DragFloat("Range", &point_lights[i].range, 0.1f, 0.0f, 10.0f);
			ImGui::PopID();

			ImGui::Separator();
		}

		if (deferred) {

			static float image_width = 512;

			ImGui::SliderFloat("Image Width", &image_width, 128.0f, 1024.0f);

			for (uint32_t attacment_index = 0; attacment_index < gbuffer_framebuffer.color_attachments.size(); attacment_index++) {

				float ratio = float(width)/ float(height);
				float image_height = image_width / ratio;

				ImGui::Image((ImTextureID)gbuffer_framebuffer.color_attachments[attacment_index].id, ImVec2(image_width, image_height), ImVec2(0, 1), ImVec2(1, 0));
				//ImGui::SameLine();
				//if (attacment_index % 2 != 0) {
				//	ImGui::NewLine();
				//}
			}
		}

		ImGui::End();


		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		// --------------- ImGui ----------------------- //
		glfwSwapBuffers(window);
	}

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwTerminate();

	return 0;
}

void gl_debug_message_callback(GLenum source,
	GLenum type,
	GLuint id,
	GLenum severity,
	GLsizei length,
	const GLchar* message,
	const void* userParam)
{
	const char* _source;
	const char* _type;
	const char* _severity;

	switch (source) {
	case GL_DEBUG_SOURCE_API:
		_source = "API";
		break;

	case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
		_source = "WINDOW SYSTEM";
		break;

	case GL_DEBUG_SOURCE_SHADER_COMPILER:
		_source = "SHADER COMPILER";
		break;

	case GL_DEBUG_SOURCE_THIRD_PARTY:
		_source = "THIRD PARTY";
		break;

	case GL_DEBUG_SOURCE_APPLICATION:
		_source = "APPLICATION";
		break;

	case GL_DEBUG_SOURCE_OTHER:
		_source = "UNKNOWN";
		break;

	default:
		_source = "UNKNOWN";
		break;
	}

	switch (type) {
	case GL_DEBUG_TYPE_ERROR:
		_type = "ERROR";
		break;

	case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
		_type = "DEPRECATED BEHAVIOR";
		break;

	case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
		_type = "UDEFINED BEHAVIOR";
		break;

	case GL_DEBUG_TYPE_PORTABILITY:
		_type = "PORTABILITY";
		break;

	case GL_DEBUG_TYPE_PERFORMANCE:
		_type = "PERFORMANCE";
		break;

	case GL_DEBUG_TYPE_OTHER:
		_type = "OTHER";
		break;

	case GL_DEBUG_TYPE_MARKER:
		_type = "MARKER";
		break;

	default:
		_type = "UNKNOWN";
		break;
	}

	switch (severity) {
	case GL_DEBUG_SEVERITY_HIGH:
		_severity = "HIGH";
		break;

	case GL_DEBUG_SEVERITY_MEDIUM:
		_severity = "MEDIUM";
		break;

	case GL_DEBUG_SEVERITY_LOW:
		_severity = "LOW";
		break;

	case GL_DEBUG_SEVERITY_NOTIFICATION:
		_severity = "NOTIFICATION";
		break;

	default:
		_severity = "UNKNOWN";
		break;
	}

	printf("%d: %s of %s severity, raised from %s: %s\n",
		id, _type, _severity, _source, message);
}

void EnableDebugMessages()
{
	glDebugMessageCallback(gl_debug_message_callback, 0);
	glEnable(GL_DEBUG_OUTPUT);
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_FALSE);
	glDebugMessageControl(GL_DEBUG_SOURCE_API, GL_DEBUG_TYPE_ERROR, GL_DONT_CARE, 0, NULL, GL_TRUE);
}