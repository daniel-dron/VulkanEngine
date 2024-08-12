#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

#include "bindless.glsl"
#include "input_structures.glsl"
#include "push_constants.glsl"

layout (location = 0) out vec3 out_normal;
layout (location = 1) out vec2 out_uvs;
layout (location = 2) out vec3 out_frag_pos;
layout (location = 3) out mat3 out_tbn;

void main() 
{
	Vertex v = pc.vertexBuffer.vertices[gl_VertexIndex];
	Material material = pc.scene.materials.mat[pc.material_id];
	
	vec4 position = vec4(v.position, 1.0f);

	gl_Position = pc.scene.viewproj * pc.model *position;

	out_uvs.x = v.uv_x;
	out_uvs.y = v.uv_y;
	out_frag_pos = vec4(pc.model * position).xyz;

	out_normal = normalize(vec3(pc.model * vec4(v.normal, 0.0f)));//normalize(mat3(transpose(inverse(PushConstants.model))) * v.normal);//(PushConstants.model * vec4(v.normal, 0.f)).xyz;
	vec3 tangent = normalize(vec3(pc.model * vec4(v.tangent.rgb, 0.0f)));
	// out_tangent = v.tangent.rgb;
	vec3 bitangent = cross(out_normal, tangent);

	out_tbn = mat3(tangent, bitangent, out_normal);
}
