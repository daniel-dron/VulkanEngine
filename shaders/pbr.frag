
#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require

#include "bindless.glsl"
#include "input_structures.glsl"
#include "scene.glsl"

layout( push_constant ) uniform constants {
    SceneBuffer scene;
    uint albedo_tex;
    uint normal_tex;
    uint position_tex;
    uint pbr_tex;
} pc;

layout (location = 0) in vec2 in_uvs;

layout (location = 0) out vec4 out_color;

void main() {
    out_color = sampleTexture2DLinear(pc.albedo_tex, in_uvs);
}