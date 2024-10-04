#version 450 core

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

#include "input_structures.glsl"
#include "vertex.glsl"

layout( push_constant ) uniform constants {
    mat4 projection;
    mat4 view;
    mat4 model;
    VertexBuffer vertex_buffer;
} pc;

void main()
{
	Vertex v = pc.vertex_buffer.vertices[gl_VertexIndex];
    gl_Position = pc.projection * pc.view * pc.model * vec4(v.position, 1.0);
}  