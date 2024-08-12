#version 450

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec3 in_frag_pos;
layout (location = 4) in mat3 in_tbn;

layout (location = 0) out vec4 out_color;

void main() {
    out_color = vec4(1.0f, 1.0f, 1.0f, 1.0f);    
}