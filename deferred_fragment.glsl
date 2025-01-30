#version 460 core

layout(location = 0) out vec4 final_positions;
layout(location = 1) out vec4 final_color;
layout(location = 2) out vec4 final_normal;
layout(location = 3) out vec4 final_view_pos;
layout(location = 4) out vec4 final_orm;

layout(location = 0) in vec3 world_pos;
layout(location = 1) in vec3 view_pos_tbn;
layout(location = 2) in vec2 uv;
layout(location = 3) in mat3 tbn;


#define BASE_COLOR_MAP_INDEX 0
#define OCCLUSION_METALLIC_ROUGHNESS_MAP_INDEX 1
#define NORMAL_MAP_INDEX 2
#define EMISSIVE_MAP_INDEX 3

layout(binding = BASE_COLOR_MAP_INDEX) uniform sampler2D base_color_map;
layout(binding = NORMAL_MAP_INDEX) uniform sampler2D normal_map;
layout(binding = OCCLUSION_METALLIC_ROUGHNESS_MAP_INDEX) uniform sampler2D orm_map;
layout(binding = EMISSIVE_MAP_INDEX) uniform sampler2D emissive_map;

void main() {
    vec4 base_color_sample = texture(base_color_map, uv);
    vec3 normal_sample = texture(normal_map, uv).rgb * 2.0 - 1.0;
    vec4 orm_sample = texture(orm_map, uv);
    vec3 emissive_sample = texture(emissive_map, uv).rgb;

	final_positions = vec4(world_pos, 1.0);
	final_color = base_color_sample;
	final_normal = normalize(vec4(tbn * normal_sample, 1.0));
	final_orm = orm_sample;
	final_view_pos = vec4(view_pos_tbn.xyz, 1.0);

}