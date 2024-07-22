struct PointLight {
	vec4 color; // w is power in W's
	float diffuse;
	float specular;
	float radius;
	float _pad;
};

layout(set = 0, binding = 0) uniform SceneData{   
	mat4 view;
	mat4 proj;
	mat4 viewproj;
	vec3 camera_position;
	int number_of_lights;
	PointLight pointLights[10];
} sceneData;

layout(set = 1, binding = 0) uniform GLTFMaterialData{   
	vec4 colorFactors;
	vec4 metal_rough_factors;
} materialData;

layout(set = 1, binding = 1) uniform sampler2D colorTex;
layout(set = 1, binding = 2) uniform sampler2D metalRoughTex;
