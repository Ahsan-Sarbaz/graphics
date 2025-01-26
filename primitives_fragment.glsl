#version 460 core

layout(location = 0) out vec4 final_color;


layout(std140, binding = 1) uniform PerObject {
    mat4 model;
    mat4 normal_matrix;
    vec4 base_color;
    vec4 emissive_color;
    vec4 specular_color;
};


void main() {    
    final_color = vec4(base_color.xyz, 1.0);
}