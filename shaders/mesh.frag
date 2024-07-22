#version 450

#extension GL_GOOGLE_include_directive : require
#include "input_structures.glsl"

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec3 in_frag_pos;

layout (location = 0) out vec4 outFragColor;

void main() 
{
	vec3 norm = normalize(inNormal);
	vec3 light_dir = normalize(scene_data.pointLights[0].position.xyz - in_frag_pos);

	float diff = max(dot(norm, light_dir), 0.0f);
	vec3 diffuse = diff * scene_data.pointLights[0].color.rgb;

	vec3 result = (diffuse) * texture(colorTex, inUV).rgb;

	outFragColor = vec4(result, 1.0f);
}
