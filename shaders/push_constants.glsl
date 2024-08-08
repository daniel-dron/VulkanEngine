#extension GL_EXT_scalar_block_layout: require
#extension GL_EXT_buffer_reference : require

#include "scene.glsl"
#include "vertex.glsl"

layout( push_constant ) uniform constants
{
	mat4 model;
	SceneBuffer scene;
	VertexBuffer vertexBuffer;
} pc;