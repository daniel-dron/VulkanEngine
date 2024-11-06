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

layout( push_constant, scalar, buffer_reference_align = 8) uniform constants {
	mat4 WorldFromLocal;
	VertexBuffer VertexBufferAddress;
	SceneBuffer scene;
	PerDrawDataList draw_data;
} pc;


layout(set = 1, binding = 0) readonly buffer SBPerDrawDataBuffer {
	PerDrawData datas[];
} sbo;

layout (location = 0) out vec2 out_uvs;
layout (location = 1) out vec3 out_frag_pos;
layout (location = 2) out vec3 out_normal;
layout (location = 3) flat out int out_material_id;
layout (location = 4) out mat3 out_tbn;

void main()
{
	PerDrawData data = sbo.datas[gl_DrawIDARB];
//    PerDrawData data = pc.draw_data.datas[gl_DrawIDARB];

    Vertex v = data.VertexBufferAddress.vertices[gl_VertexIndex];

	vec4 position = vec4(v.Position.xyz, 1.0f);
	gl_Position = pc.scene.viewproj * data.WorldFromLocal * position;

	Material material = pc.scene.materials.mat[data.MaterialId];
    out_material_id = data.MaterialId;

	out_uvs = vec2(v.Position.w, v.Normal.w);
	out_frag_pos = (data.WorldFromLocal * position).xyz;

	vec3 T = normalize(vec3(data.WorldFromLocal * vec4(v.Tangent.xyz, 0.0)));
	vec3 B = normalize(vec3(data.WorldFromLocal * vec4(v.Bitangent.xyz, 0.0)));
	vec3 N = normalize(vec3(data.WorldFromLocal * vec4(v.Normal.xyz, 0.0)));
	out_tbn = mat3(T, B, N);

	out_normal = mat3(transpose(inverse(data.WorldFromLocal))) * v.Normal.xyz;
}
