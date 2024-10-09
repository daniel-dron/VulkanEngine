#include "materials.glsl"

struct PointLight {
	vec3 position;
	float constant;
	vec3 color;
	float linear;
	float quadratic;
	float pad1;
	float pad2;
	float pad3;
};

struct DirectionalLight {
	vec3 direction;
	int pad1;
	vec4 color;
};

layout (buffer_reference, scalar) readonly buffer SceneBuffer {
	mat4 view;
	mat4 proj;
	mat4 viewproj;
	mat4 light_proj;
	mat4 light_view;
  	vec4 fog_color;
	vec3 camera_position;
	float ambient_light_factor;
	vec3 ambient_light_color;
  	float fog_end;
  	float fog_start;
    MaterialsBuffer materials;
	int number_of_directional_lights;
	int number_of_point_lights;
	int shadowmap;
} sceneBuffer;