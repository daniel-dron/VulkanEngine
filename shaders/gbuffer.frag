#version 450

#extension GL_GOOGLE_include_directive : require

#include "bindless.glsl"
#include "input_structures.glsl"
#include "push_constants.glsl"

layout (location = 0) in vec2 in_uvs;
layout (location = 1) in vec3 in_frag_pos;
layout (location = 2) in mat3 in_tbn;

layout (location = 0) out vec4 out_albedo;
layout (location = 1) out vec4 out_normal;
layout (location = 2) out vec4 out_position;
layout (location = 3) out vec4 out_pbr;

void main() {
	Material material = pc.scene.materials.mat[pc.material_id];

    out_albedo = sampleTexture2DNearest(material.color_tex, in_uvs);

	vec3 norm = sampleTexture2DNearest(material.normal_tex, in_uvs).rgb;
	norm = normalize(norm * 2.0f - 1.0f);
	norm = normalize(in_tbn * norm);
    out_normal = vec4(norm, 1.0f);
    
    out_position = vec4(in_frag_pos, 1.0f);

    vec4 pbr_factors = material.metal_roughness_factors;
    vec4 pbr_sample = sampleTexture2DNearest(material.metal_roughness_tex, in_uvs);

    out_pbr = pbr_factors * pbr_sample;
    out_pbr.a = 1.0f;
}