#version 450

#extension GL_GOOGLE_include_directive : require

#include "imgui_push_constants.glsl"

layout (location = 0) out vec4 out_color;
layout (location = 1) out vec2 out_uvs;

void main() {
    const Vertex v = pc.vertex_buffer.vertices[gl_VertexIndex];
    const vec4 color = unpackUnorm4x8(v.color);
    out_color = color;
    out_uvs = v.uvs;

    gl_Position = vec4(v.position * pc.scale + pc.offset, 0.0f, 1.0f);
}