#version 460 core
#extension GL_NV_gpu_shader5 : enable

struct Vertex {
    f16vec4 position;
    u8vec4 normal;
    u8vec4 tangent;
    f16vec2 uv;
};

layout(std430, binding = 0) readonly buffer VertexBuffer {
    Vertex vertices[];
};

layout(std140, binding = 0) uniform PerFrame {
    mat4 view;
    mat4 projection;
    vec3 camera_position;
};

layout(std140, binding = 1) uniform PerObject {
    mat4 model;
    mat4 normal_matrix;
    vec4 base_color;
	vec4 emissive_color;
	vec4 specular_color;
};

layout(location = 0) out vec3 world_pos;
layout(location = 1) out vec3 view_pos_tbn;
layout(location = 2) out vec2 uv;
layout(location = 3) out mat3 tbn;

void main() {    
    Vertex vertex = vertices[gl_VertexID];

    vec4 pos = model * vec4(vertex.position);
    
    gl_Position = projection * view * pos;
	
	world_pos = pos.xyz;

	vec3 N = normalize((normal_matrix * (vec4(vertex.normal.xyzw) / 127.0 - 1.0)).xyz);
	vec3 T = normalize((normal_matrix * (vec4(vertex.tangent.xyzw) / 127.0 - 1.0)).xyz);
	float handedness = float(vertex.tangent.w) / 127.0 - 1.0;
	vec3 B = normalize(cross(N, T));

	mat3 TBN = mat3(T, B, N);

	mat3 invTBN = transpose(TBN);

	view_pos_tbn = invTBN * (camera_position - pos.xyz);
	
	tbn = TBN;

    uv = vertex.uv;
}