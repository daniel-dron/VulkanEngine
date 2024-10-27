
#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require

#include "bindless.glsl"
#include "input_structures.glsl"
#include "scene.glsl"

layout (set = 1, binding = 0) uniform IBL {
    float irradiance_factor;
    float radiance_factor;
    float brdf_factor;
    int pad;
} ibl;

layout (set = 1, binding = 1) uniform DirectionalLights {
    DirectionalLight lights[10];
} directionalLights;

layout (set = 1, binding = 2) uniform PointLights {
    PointLight lights[10];
} pointLights;

layout( push_constant ) uniform constants {
    SceneBuffer scene;
    uint albedo_tex;
    uint normal_tex;
    uint position_tex;
    uint pbr_tex;
    uint irradiance_map;
    uint radiance_map;
    uint brdf_lut;
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

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec4 sampleTextureCubeLinearLod(uint texID, vec3 p, float lod) {
    return textureLod(nonuniformEXT(samplerCube(textureCubes[texID], samplers[LINEAR_SAMPLER_ID])), p, lod);
}

float CalculateShadowForDirectionalLight(DirectionalLight light, vec3 position_world) {
    vec4 light_space_pos = light.proj * light.view * vec4(position_world, 1.0);

    vec3 projCoords = light_space_pos.xyz / light_space_pos.w;
    float current_depth = projCoords.z;

    projCoords = projCoords * 0.5f + 0.5f;
    float bias = 0.005;
    // float closest_depth = sampleTexture2DLinear(pc.scene.shadowmap, projCoords.xy).r + bias;

    float shadow = 0.0f;
    vec2 texelSize = vec2(1.0 / 2048.0f);
    for(int x = -2; x <= 2; ++x)
    {
        for(int y = -2; y <= 2; ++y)
        {
            float closest_depth = sampleTexture2DLinear(light.shadowmap, projCoords.xy + vec2(x, y) * texelSize).r + bias;
            shadow += current_depth > closest_depth ? 1.0f : 0.0f;
        }
    }
    shadow = shadow / 25.0f;
    shadow = 1.0f - shadow;

    return shadow;
}

vec3 pbr(vec3 albedo, vec3 emissive, float metallic, float roughness, float ao, vec3 normal, vec3 view_dir) {
    vec3 N = normal;
    vec3 V = view_dir;
    vec3 R = reflect(-V, N); 

    vec3 position = sampleTexture2DLinear(pc.position_tex, in_uvs).rgb;

	// interpolate surface reflection between 0.04 (minimum) and the albedo value
	// in relation to the metallic factor of the material
	vec3 F0 = vec3(0.04); 
	F0      = mix(F0, albedo, metallic);

    vec3 Lo = vec3(0.0f);
    for (int i = 0; i < pc.scene.number_of_directional_lights; i++) {
        DirectionalLight light = directionalLights.lights[i];
        vec3 L = normalize(light.direction);  // Note the negative sign
        vec3 H = normalize(V + L);

        // No attenuation for directional lights
        vec3 radiance = light.color.rgb;

        vec3 F  = fresnelSchlick(max(dot(H, V), 0.0), F0);

        float NDF = DistributionGGX(N, H, roughness);
        float G   = GeometrySmith(N, V, L, roughness);

        vec3 numerator    = NDF * G * F;
        float denominator = 4.0f * max(dot(N, V), 0.0f) * max(dot(N, L), 0.0f) + 0.0001f;
        vec3 specular     = numerator / denominator;

        vec3 kS = F;
        vec3 kD = vec3(1.0f) - kS;

        kD *= 1.0f - metallic;

        float NdotL = max(dot(N, L), 0.0f);
        float theta_max = 0.5 * 3.14159 / 180.0; // 0.5 degrees in radians
        float pdf = 1.0 / (2.0 * 3.14159 * (1.0 - cos(theta_max)));

        // Divide by PDF
        float s = CalculateShadowForDirectionalLight(light, position);

        Lo += (kD * albedo / PI + specular) * radiance * NdotL * s;
    }

    for (int i = 0; i < pc.scene.number_of_point_lights; i++) {
        PointLight light = pointLights.lights[i];
        vec3 L = normalize(light.position - position);
        vec3 H = normalize(V + L);

        float distance = length(light.position - position);
        float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));
        vec3 radiance = light.color.rgb * attenuation;

        vec3 F  = fresnelSchlick(max(dot(H, V), 0.0), F0);

        float NDF = DistributionGGX(N, H, roughness);
        float G   = GeometrySmith(N, V, L, roughness);

        vec3 numerator    = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3 specular     = numerator / denominator;

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;

        kD *= 1.0 - metallic;

        float NdotL = max(dot(N, L), 0.0);
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    //vec3 kS = fresnelSchlick(max(dot(N, V), 0.0f), F0);
    vec3 F = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);
    vec3 kS = F;
    vec3 kD = 1.0f - kS;
    kD *= 1.0f - metallic;
    vec3 irradiance = sampleTextureCubeNearest(pc.irradiance_map, normal).rgb * ibl.irradiance_factor;
    vec3 diffuse = irradiance * albedo;

    // sample both the pre-filter map and the BRDF lut and combine them together as per the Split-Sum approximation to get the IBL specular part.
    const float MAX_REFLECTION_LOD = 4.0;
    vec3 radiance = sampleTextureCubeLinearLod(pc.radiance_map, R, roughness * MAX_REFLECTION_LOD).rgb * ibl.radiance_factor;
    float rough = clamp(roughness, 0.01, 0.99);
    vec2 brdf  = sampleTexture2DLinear(pc.brdf_lut, vec2(max(dot(N, V), 0.001f), rough)).rg * ibl.brdf_factor;
    vec3 specular = radiance * (F * brdf.x + brdf.y);

    vec3 ambient = (kD * diffuse + specular) * ao;
    vec3 color = ambient + Lo + emissive;

    return color;
}

vec4 reconstructWorldPosition(vec2 screenPos, float depth, mat4 invView, mat4 invProj, vec2 screenSize) {
    vec2 ndc;
    ndc.x = (2.0 * screenPos.x) / screenSize.x - 1.0;
    ndc.y = 1.0 - (2.0 * screenPos.y) / screenSize.y;
    vec4 clipSpacePos = vec4(ndc, depth, 1.0);
    vec4 viewSpacePos = invProj * clipSpacePos;
    viewSpacePos /= viewSpacePos.w;
    vec4 worldSpacePos = invView * viewSpacePos;
    
    return worldSpacePos;
}

void main() {
    vec3 albedo = sampleTexture2DLinear(pc.albedo_tex, in_uvs).rgb;
    vec3 normal = sampleTexture2DLinear(pc.normal_tex, in_uvs).rgb;
    vec4 position = sampleTexture2DLinear(pc.position_tex, in_uvs).xyzw;
    vec4 pbr_values = sampleTexture2DLinear(pc.pbr_tex, in_uvs);
    
    vec3 view_dir = normalize(pc.scene.camera_position.xyz - position.xyz);

    float roughness = pbr_values.g;
    float metallic = pbr_values.b;

    vec3 color = pbr(albedo, vec3(0.0f, 0.0f, 0.0f), metallic, roughness, 1.0f, normal, view_dir);
    out_color = vec4(color, 1.0f);
}