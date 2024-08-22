#version 450

#extension GL_GOOGLE_include_directive : require

#include "bindless.glsl"
#include "skybox_push_constants.glsl"

layout (location = 0) in vec3 in_pos;

layout (location = 0) out vec4 out_color;

void main( ) {
    out_color = sampleTextureCubeLinear(pc.skybox_texture, in_pos);
    out_color.a = 1.0f;
}