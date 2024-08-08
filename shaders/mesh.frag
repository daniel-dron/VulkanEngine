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
	vec3 camera_position = pc.scene.camera_position;

	// vec3 norm = normalize(inNormal);
	vec3 norm = texture(normalTex, inUV).rgb;
	norm = normalize(norm * 2.0f - 1.0f);
	norm = normalize(in_tbn * norm);

	vec3 light_dir = normalize(scene_data.pointLights[0].position.xyz - in_frag_pos);
	vec3 view_dir = normalize(camera_position - in_frag_pos);
	vec3 reflect_dir = reflect(-light_dir, norm);

	// ----------
	// ambient
	float ambient_factor = toLinear(vec4(scene_data.ambient_light_factor, scene_data.ambient_light_factor, scene_data.ambient_light_factor, 1.0f)).r;
	vec3 ambient = scene_data.ambient_light_color * ambient_factor;

	// ----------
	// diffuse
	float diff = max(dot(norm, light_dir), 0.0f);
	float light_diffuse_factor = scene_data.pointLights[0].diffuse;
	vec3 diffuse = toLinear(vec4(light_diffuse_factor, light_diffuse_factor, light_diffuse_factor, 1.0f)).r
					* diff * scene_data.pointLights[0].color.rgb;

	// ----------
	// specular
	float light_specular_factor = scene_data.pointLights[0].specular;
	float spec = pow(max(dot(view_dir, reflect_dir), 0.0f), 32.0f);
	vec3 specular = toLinear(vec4(light_specular_factor, light_specular_factor, light_specular_factor, 1.0f)).r
					* spec * scene_data.pointLights[0].color.rgb;

	// ----------
	// fog
	float fog_end = scene_data.fog_end;
	float fog_start = scene_data.fog_start;
	float cam_to_frag_dist = length(in_frag_pos - camera_position);
	float range = fog_end - fog_start;
	float dist = fog_end - cam_to_frag_dist;
	float fog_factor = dist / range;
	fog_factor = clamp(fog_factor, 0.0f, 1.0f);

	vec4 color = sampleTexture2DNearest(55, inUV);

	vec3 result = (ambient + diffuse + specular) * color.rgb;
	result = mix(scene_data.fog_color, vec4(result, 1.0f), fog_factor).rgb;

	outFragColor = vec4(result, 1.0f);
}
