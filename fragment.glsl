#version 460 core

layout(location = 0) out vec4 final_color;

layout(location = 0) in vec3 world_pos;
layout(location = 1) in vec3 camera_position_tbn;
layout(location = 2) in vec2 uv;
layout(location = 3) in mat3 tbn;

struct DirectionalLight {
    vec4 direction_intensity; // intensity in w
    vec3 color;
};

struct PointLight {
    vec4 position_range; // range in w
    vec4 color_intensity; // intensity in w
};

struct SpotLight {
    vec4 position_range; // range in w
    vec4 direction_intensity; // intensity in w
    vec4 color_angle; // angle in w
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

layout(std140, binding = 2) uniform u_DirectionalLight {
    DirectionalLight sun;
};

layout(std140, binding = 3) uniform u_PointLights {
    PointLight point_lights[4];
};

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

layout(binding = BASE_COLOR_MAP_INDEX) uniform sampler2D base_color_map;
layout(binding = NORMAL_MAP_INDEX) uniform sampler2D normal_map;
layout(binding = AMBIENT_OCCLUSION_MAP_INDEX) uniform sampler2D occlussion_roughness_map;
layout(binding = EMISSIVE_MAP_INDEX) uniform sampler2D emissive_map;

// Constants defined at compile time
const float PI = 3.14159265359;
const float EPSILON = 1e-6;
const vec3 F0_NON_METAL = vec3(0.04);

// Optimized Fresnel-Schlick
vec3 F_Schlick(float cos_theta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - cos_theta, 5.0);
}

// Optimized GGX/Trowbridge-Reitz Normal Distribution
float D_GGX(float NoH, float roughness) {
    float alpha = roughness * roughness;
    float alpha_sq = alpha * alpha;
    float denom = NoH * NoH * (alpha_sq - 1.0) + 1.0;
    return alpha_sq / (PI * denom * denom);
}

// Optimized Schlick-GGX Geometry Function
float G_Smith(float NoV, float NoL, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    float ggx1 = NoV / (NoV * (1.0 - k) + k);
    float ggx2 = NoL / (NoL * (1.0 - k) + k);
    return ggx1 * ggx2;
}

// Compute point light contribution using PBR BRDF
vec3 point_light_radiance(PointLight light, vec3 world_pos, vec3 albedo, vec3 N, vec3 V, float NoV, float metallic, float roughness, vec3 F0)
{
    vec3 L = light.position_range.xyz - world_pos;
    float distance = length(L);
    
    // Attenuation based on light range
    float range = light.position_range.w;
    float attenuation = max(0.0, 1.0 - distance / range);
    attenuation *= attenuation; // Quadratic falloff
    
    // Skip computation if outside light range
    if (attenuation < EPSILON) {
        return vec3(0.0);
    }
    
    L = normalize(L);
    vec3 H = normalize(V + L);

    float NoL = max(dot(N, L), EPSILON);
    float NoH = max(dot(N, H), EPSILON);
    float LoH = max(dot(L, H), EPSILON);

    // Cook-Torrance BRDF terms
    float D = D_GGX(NoH, roughness);
    float G = G_Smith(NoV, NoL, roughness);
    vec3 F = F_Schlick(LoH, F0);

    // Combine terms
    vec3 spec = (D * G * F) / (4.0 * NoV * NoL + EPSILON);
    vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
    
    // Final lighting computation
    vec3 diffuse = kD * albedo / PI;
    vec3 point_light_radiance = light.color_intensity.rgb * light.color_intensity.w;
    vec3 direct_light = (diffuse + spec) * point_light_radiance * NoL * attenuation;
    
    return direct_light;
}

vec3 directional_light_radiance(vec3 albedo, vec3 N, vec3 V, float NoV, float metallic, float roughness, vec3 F0)
{
    vec3 L = normalize(-sun.direction_intensity.xyz);
    vec3 H = normalize(V + L);

    float NoL = max(dot(N, L), EPSILON);
    float NoH = max(dot(N, H), EPSILON);
    float LoH = max(dot(L, H), EPSILON);

    // Cook-Torrance BRDF terms - computed only once
    float D = D_GGX(NoH, roughness);
    float G = G_Smith(NoV, NoL, roughness);
    vec3 F = F_Schlick(LoH, F0);

    // Combine terms
    vec3 spec = (D * G * F) / (4.0 * NoV * NoL + EPSILON);
    vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
    
    // Final lighting computation
    vec3 diffuse = kD * albedo / PI;
    vec3 sun_radiance = sun.color * sun.direction_intensity.w;
    vec3 direct_light = (diffuse + spec) * sun_radiance * max(NoL, 0.0);
    return direct_light;
}

void main() {
    // Sample textures - done once to avoid multiple texture reads
    vec4 albedo_sample = texture(base_color_map, uv);

    // Early discard for fully transparent pixels
    if (albedo_sample.a < EPSILON) {
        discard;
    }
    
    vec3 normal_sample = texture(normal_map, uv).rgb * 2.0 - 1.0;
    vec4 mr_sample = texture(occlussion_roughness_map, uv);
    float ao = mr_sample.r;

    // Transform normal and compute essential vectors - normalized once
    vec3 N = normalize(tbn * normal_sample);
    vec3 V = normalize(camera_position_tbn);

    // Compute dot products once and cache them
    float NoV = max(dot(N, V), EPSILON);

    // Material properties
    float metallic = mr_sample.b;
    float roughness = mr_sample.g; 
    
    // Calculate F0 (specular reflection at zero incidence)
    vec3 F0 = mix(F0_NON_METAL, albedo_sample.rgb, metallic);

    // Directional light contribution
    vec3 sun_light = directional_light_radiance(albedo_sample.rgb, N, V, NoV, metallic, roughness, F0);

    // Point lights contribution
    vec3 point_lights_contribution = vec3(0.0);
    for (int i = 0; i < 4; ++i) {
        point_lights_contribution += point_light_radiance(
            point_lights[i], 
            world_pos, 
            albedo_sample.rgb, 
            N, 
            V, 
            NoV, 
            metallic, 
            roughness, 
            F0
        );
    }

    // Ambient term
    float ambient_intensity = 0.1;
    vec3 ambient = albedo_sample.rgb * ambient_intensity * ao;

    // Combine lighting contributions
    vec3 final = sun_light + point_lights_contribution + ambient;
    
    final_color = vec4(final, 1.0);
}