#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_ARB_shader_draw_parameters : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require

#include "bindless.glsl"
#include "input_structures.glsl"
#include "scene.glsl"

struct Vertex {
    vec4 Position;   // 16 bytes (vec3 position + float uv_x)
    vec4 Normal;     // 16 bytes (vec3 normal + float uv_y)
    vec4 Tangent;    // 16 bytes (vec3 tangent + padding)
    vec4 Bitangent;  // 16 bytes (vec3 bitangent + padding)
};

layout(buffer_reference, scalar, buffer_reference_align = 8) readonly buffer VertexBuffer {
    Vertex vertices[];
};

struct PerDrawData {
	mat4 			WorldFromLocal;
	VertexBuffer	VertexBufferAddress;
	int             MaterialId;
};

layout(buffer_reference, scalar, buffer_reference_align = 8) readonly buffer PerDrawDataList {
	PerDrawData datas[];
};

layout( push_constant, scalar ) uniform constants {
	SceneBuffer scene;
	PerDrawDataList draw_data;
} pc;

layout (location = 0) in vec2 in_uvs;
layout (location = 1) in vec3 in_frag_pos;
layout (location = 2) in vec3 in_normal;
layout (location = 3) flat in int in_material_id;
layout (location = 4) in mat3 in_tbn;

layout (location = 0) out vec4 out_albedo;
layout (location = 1) out vec4 out_normal;
layout (location = 2) out vec4 out_position;
layout (location = 3) out vec4 out_pbr;

void main() {
    Material material = pc.scene.materials.mat[in_material_id];

    if (material.TextureIndices[0] == 0xfffffffe) {
        out_albedo = material.BaseColor;
    } else {
        out_albedo = toLinear(sampleTexture2DLinear(material.TextureIndices[0], in_uvs));
        if (out_albedo.a < 0.1f) {
            discard;
        }
    }

    if (material.TextureIndices[2] == 0xfffffffe) {
        out_normal = vec4(in_normal, 1.0f);
    } else {
        vec3 norm = sampleTexture2DLinear(material.TextureIndices[2], in_uvs).rgb;
        norm = normalize(norm * 2.0f - 1.0f);
        norm = normalize(in_tbn * norm);
        out_normal = vec4(norm, 1.0f);
    }

    out_position = vec4(in_frag_pos, 1.0f);

    if (material.TextureIndices[1] != 0xfffffffe) {
        vec4 pbr_factors = material.Factors;
        vec4 pbr_sample = sampleTexture2DLinear(material.TextureIndices[1], in_uvs);

        out_pbr = pbr_sample;
        out_pbr.g = out_pbr.g * pbr_factors.g;
        out_pbr.b = out_pbr.b * pbr_factors.r;
        out_pbr.a = 1.0f;
    } else {
        vec4 pbr_factors = material.Factors;
        out_pbr.r = 1.0f;
        out_pbr.g = pbr_factors.g;
        out_pbr.b = pbr_factors.r;
        out_pbr.a = 1.0f;
    }
}