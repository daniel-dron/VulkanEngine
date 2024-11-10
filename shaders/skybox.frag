#version 450

#extension GL_GOOGLE_include_directive : require

#include "common/bindless.glsl"
#include "skybox_push_constants.glsl"

layout (location = 0) in vec3 in_pos;

layout (location = 0) out vec4 out_color;

const vec2 invAtan = vec2(0.1591, 0.3183);
vec2 SampleSphericalMap(vec3 v) {
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= invAtan;
    uv += 0.5;
    return uv;
}

void main( ) {
    vec3 color = sampleTextureCubeNearest(pc.skybox_texture, in_pos).rgb;

    out_color.rgb = color;
    out_color.a = 1.0f;
}