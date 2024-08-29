
#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require

#include "bindless.glsl"
#include "input_structures.glsl"
#include "scene.glsl"

layout( push_constant ) uniform constants {
    SceneBuffer scene;
    uint albedo_tex;
    uint normal_tex;
    uint position_tex;
    uint pbr_tex;
} pc;

layout (location = 0) in vec2 in_uvs;

layout (location = 0) out vec4 out_color;

#define PI 3.1415926538

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a      = roughness*roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
	
    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
	
    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	
    return num / denom;
}
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);
	
    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
} 

void main() {
    vec3 albedo = sampleTexture2DLinear(pc.albedo_tex, in_uvs).rgb;
    vec3 normal = sampleTexture2DLinear(pc.normal_tex, in_uvs).rgb;
    vec3 position = sampleTexture2DLinear(pc.position_tex, in_uvs).rgb;
    vec4 pbr_values = sampleTexture2DLinear(pc.pbr_tex, in_uvs);
    
    vec3 view_dir = normalize(pc.scene.camera_position - position);

    float roughness = pbr_values.g;
    float metallic = pbr_values.b;

    vec3 Lo = vec3(0.0f);
    for (int i = 0; i < pc.scene.number_of_lights; i++) {
        PointLight light = pc.scene.pointLights[i];
        vec3 light_dir = normalize(light.position - position);
        vec3 halfway = normalize(light_dir + view_dir);

        float distance = length(light.position - position);
        float attenuation = 1.0f / (distance * distance);
        vec3 radiance = light.color.rgb * attenuation;

        // ---------
        // Cook Torrance BRDF

        vec3 F0 = vec3(0.04); // non metallic is always 0.04f
        F0      = mix(F0, albedo, metallic);
        vec3 F  = fresnelSchlick(max(dot(halfway, view_dir), 0.0), F0);

        float NDF = DistributionGGX(normal, halfway, roughness);

        float G   = GeometrySmith(normal, view_dir, light_dir, roughness);

        vec3 numerator    = NDF * G * F;
        float denominator = 4.0 * max(dot(normal, view_dir), 0.0) * max(dot(normal, light_dir), 0.0)  + 0.0001;
        vec3 specular     = numerator / denominator;

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        
        kD *= 1.0 - metallic;

        float NdotL = max(dot(normal, light_dir), 0.0);
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    vec3 ambient = vec3(0.03) * albedo;
    vec3 color = ambient + Lo;

    color = color / (color + vec3(1.0));

    out_color = vec4(color, 1.0f);
}