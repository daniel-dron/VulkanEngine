#version 450

#extension GL_GOOGLE_include_directive : require

#include "common/bindless.glsl"
#include "imgui_push_constants.glsl"
#include "input_structures.glsl"

layout (location = 0) in vec4 in_color;
layout (location = 1) in vec2 in_uvs;

layout (location = 0) out vec4 out_color;

void main() {
    vec4 color = in_color * sampleTexture2DNearest(pc.texture_id, in_uvs);
    color.rgb *= color.a;

    if (pc.is_srgb != 0) {
        color = toLinear(color);
        color.a = 1.0 - gammaToLinear(1 - color.a);
    }

    out_color = color;
}
