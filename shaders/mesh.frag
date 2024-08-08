#version 450

#extension GL_GOOGLE_include_directive : require

#include "bindless.glsl"
#include "input_structures.glsl"
#include "push_constants.glsl"

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec3 in_frag_pos;
layout (location = 4) in mat3 in_tbn;

layout (location = 0) out vec4 outFragColor;

void main() 
{
	Material material = pc.scene.materials.mat[pc.material_id];

	// vec3 norm = normalize(inNormal);
	vec3 norm = sampleTexture2DNearest(material.normal_tex, inUV).rgb;
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

	// ----------
	// fog
	float fog_end = pc.scene.fog_end;
	float fog_start = pc.scene.fog_start;
	float cam_to_frag_dist = length(in_frag_pos - pc.scene.camera_position);
	float range = fog_end - fog_start;
	float dist = fog_end - cam_to_frag_dist;
	float fog_factor = dist / range;
	fog_factor = clamp(fog_factor, 0.0f, 1.0f);

	vec4 color = sampleTexture2DNearest(material.color_tex, inUV);

	vec3 result = (ambient + diffuse + specular) * color.rgb;
	result = mix(pc.scene.fog_color, vec4(result, 1.0f), fog_factor).rgb;

	outFragColor = vec4(result, 1.0f);
}
