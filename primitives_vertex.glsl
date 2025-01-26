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


void main() {    
    Vertex vertex = vertices[gl_VertexID];

    vec4 pos = model * vec4(vertex.position);
    
    gl_Position = projection * view * pos;	
}