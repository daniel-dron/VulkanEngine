#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require

#include "bindless.glsl"
#include "skybox_push_constants.glsl"

layout (location = 0) out vec3 out_pos;

void main() {
	Vertex v = pc.vertex_buffer.vertices[gl_VertexIndex];
    out_pos = v.position;

	// Remove translation from view matrix
	mat4 view = mat4(mat3(pc.scene.view));
	vec4 pos = pc.scene.proj * view * vec4(v.position, 1.0);
    
    gl_Position = pos.xyww;
}
