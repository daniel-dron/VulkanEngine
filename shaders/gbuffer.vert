#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

#include "bindless.glsl"
#include "input_structures.glsl"
#include "push_constants.glsl"

layout (location = 0) out vec2 out_uvs;
layout (location = 1) out vec3 out_frag_pos;
layout (location = 2) out vec3 out_normal;
layout (location = 3) out mat3 out_tbn;

void main() 
{
	Vertex v = pc.vertexBuffer.vertices[gl_VertexIndex];
	Material material = pc.scene.materials.mat[pc.material_id];
	
	vec4 position = vec4(v.Position.xyz, 1.0f);

	gl_Position = pc.scene.viewproj * pc.model * position;

	out_uvs.x = v.Position.z;
	out_uvs.y = v.Normal.z;
	out_frag_pos = (pc.model * position).xyz;

	vec3 T = normalize(vec3(pc.model * vec4(v.Tangent.xyz, 0.0)));
    vec3 B = normalize(vec3(pc.model * vec4(v.Bitangent.xyz, 0.0)));
    vec3 N = normalize(vec3(pc.model * vec4(v.Normal.xyz, 0.0)));
    out_tbn = mat3(T, B, N);

	out_normal = mat3(transpose(inverse(pc.model))) * v.Normal.xyz;
}
