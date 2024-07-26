#version 450

#extension GL_GOOGLE_include_directive : require
#include "input_structures.glsl"

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec3 in_frag_pos;
layout (location = 4) in mat3 in_tbn;

layout (location = 0) out vec4 outFragColor;

void main() 
{
	// vec3 norm = normalize(inNormal);
	vec3 norm = texture(normalTex, inUV).rgb;
	norm = normalize(norm * 2.0f - 1.0f);
	norm = normalize(in_tbn * norm);

	vec3 light_dir = normalize(scene_data.pointLights[0].position.xyz - in_frag_pos);
	vec3 view_dir = normalize(scene_data.camera_position - in_frag_pos);
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

	vec3 result = (ambient + diffuse + specular) * texture(colorTex, inUV).rgb;

	outFragColor = vec4(result, 1.0f);
}
