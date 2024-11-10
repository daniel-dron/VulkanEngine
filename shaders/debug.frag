#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require

#include "common/bindless.glsl"
#include "common/scene.glsl"

layout( location = 0 ) in vec2 in_uvs;
layout( location = 0 ) out vec4 out_color;

layout( push_constant ) uniform constants {
    SceneBuffer scene;
    uint        albedo;
    uint        normal;
    uint        position;
    uint        pbr;
    uint        render_target;
}
pc;

void main( ) {
    switch ( pc.render_target ) {
    case 0:
        out_color = vec4( 1.0f, 0.0f, 1.0f, 1.0f );
        break;
    case 1:
        out_color = vec4( sampleTexture2DLinear( pc.normal, in_uvs ).rgb * 0.5f + 0.5f, 1.0f );
        break;
    case 2:
        out_color = vec4( 0.0f, 1.0f, 0.0f, 1.0f );
        break;
    default:
        out_color = vec4( 0.0f, 0.0f, 1.0f, 1.0f );
    }
}