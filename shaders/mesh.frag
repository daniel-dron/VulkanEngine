#version 450

#extension GL_GOOGLE_include_directive : require

#include "bindless.glsl"
#include "input_structures.glsl"
#include "push_constants.glsl"

layout (location = 0) in vec2 in_uvs;
layout (location = 1) in vec3 in_frag_pos;
layout (location = 2) in mat3 in_tbn;

layout (location = 0) out vec4 outFragColor;

void main() 
{
	Material material = pc.scene.materials.mat[pc.material_id];

	vec3 norm = sampleTexture2DNearest(material.normal_tex, in_uvs).rgb;
	norm = normalize(norm * 2.0f - 1.0f);
	norm = normalize(in_tbn * norm);

	vec3 light_dir = normalize(pc.scene.pointLights[0].position.xyz - in_frag_pos);
	vec3 view_dir = normalize(pc.scene.camera_position - in_frag_pos);
	vec3 reflect_dir = reflect(-light_dir, norm);

	// ----------
	// ambient
	float ambient_factor = toLinear(vec4(pc.scene.ambient_light_factor, pc.scene.ambient_light_factor, pc.scene.ambient_light_factor, 1.0f)).r;
	vec3 ambient = pc.scene.ambient_light_color * ambient_factor;

	// ----------
	// diffuse
	float diff = max(dot(norm, light_dir), 0.0f);
	float light_diffuse_factor = pc.scene.pointLights[0].diffuse;
	vec3 diffuse = toLinear(vec4(light_diffuse_factor, light_diffuse_factor, light_diffuse_factor, 1.0f)).r
					* diff * pc.scene.pointLights[0].color.rgb;

	// ----------
	// specular
	float light_specular_factor = pc.scene.pointLights[0].specular;
	float spec = pow(max(dot(view_dir, reflect_dir), 0.0f), 32.0f);
	vec3 specular = toLinear(vec4(light_specular_factor, light_specular_factor, light_specular_factor, 1.0f)).r
					* spec * pc.scene.pointLights[0].color.rgb;

	vec4 color = sampleTexture2DNearest(material.color_tex, in_uvs);

	vec3 result = (ambient + diffuse + specular) * color.rgb;
	outFragColor = vec4(result, 1.0f);
}
