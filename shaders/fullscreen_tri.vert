#version 450

#extension GL_GOOGLE_include_directive : require

#include "bindless.glsl"
#include "input_structures.glsl"

layout (location = 0) out vec2 out_uvs;

void main() {
    out_uvs = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(out_uvs * 2.0f + -1.0f, 0.0f, 1.0f);
}