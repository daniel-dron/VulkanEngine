#version 450

#extension GL_GOOGLE_include_directive : require
#include "input_structures.glsl"

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inUV;

layout (location = 0) out vec4 outFragColor;

void main() 
{
	//outFragColor = vec4(texture(colorTex, inUV).xyz * sceneData.ambientColor.xyz, 1.0f);
	//outFragColor = vec4(sceneData.pointLights[0].color.xyz, 1.0f);
	outFragColor = vec4(inNormal.xyz, 1.0f);
}
