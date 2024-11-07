#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_multiview : require

#include "bindless.glsl"
#include "vertex.glsl"
#include "scene.glsl"

layout (buffer_reference, scalar) readonly buffer IblMatrices {
    mat4 proj;
    mat4 view[6];
} iblMatrices;

layout (push_constant) uniform constants {
    VertexBuffer vertex_buffer;
    IblMatrices iblMatrices;
    int skybox;
} pc;

layout (location = 0) out vec3 out_pos;

void main() {
	Vertex v = pc.vertex_buffer.vertices[gl_VertexIndex];

    out_pos = v.Position.xyz;
	out_pos.xy *= -1.0;

	// Remove translation from view matrix
	mat4 view = mat4(mat3(pc.iblMatrices.view[gl_ViewIndex]));
	vec4 pos = pc.iblMatrices.proj * view * vec4(v.Position.xyz, 1.0);
    
    gl_Position = pos.xyww;
}