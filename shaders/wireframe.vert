#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

#include "common/bindless.glsl"
#include "common/push_constants.glsl"

void main() 
{
	Vertex v = pc.vertexBuffer.vertices[gl_VertexIndex];
	vec4 position = vec4(v.Position.xyz, 1.0f);
	gl_Position = pc.scene.viewproj * pc.model *position;
}
