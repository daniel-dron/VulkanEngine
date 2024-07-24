#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

#include "input_structures.glsl"

layout(location = 0) out vec4 out_color;

struct Vertex {
  vec3 position;
  float uv_x;
  vec3 normal;
  float uv_y;
  vec4 color;
};

layout(buffer_reference, std430) readonly buffer VertexBuffer {
  Vertex vertices[];
};

layout(push_constant) uniform constants {
  mat4 world_from_local;
  VertexBuffer vertex_buffer;
}
pc;

void main() {
  Vertex v = pc.vertex_buffer.vertices[gl_VertexIndex];
  float desired_screen_size = 20.0f;

  vec4 local_position = vec4(v.position, 1.0f);
  vec4 pos = scene_data.viewproj * pc.world_from_local * local_position;

	pos.w = 5.0f;

  gl_Position = pos;
  out_color = vec4(1.0f, 0.0f, 0.0f, 1.0f);
}