#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require

#include "vertex.glsl"
#include "scene.glsl"

layout (push_constant) uniform constants {
    SceneBuffer scene;
    VertexBuffer vertex_buffer;
    uint skybox_texture;
} pc;