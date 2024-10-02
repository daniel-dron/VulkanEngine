#include "materials.glsl"

struct PointLight {
	vec3 position;
	float radius;
	vec4 color; // w is power in W's
	float diffuse;
	float specular;
	int pad;
	int pad2;
};

struct DirectionalLight {
	vec3 position;
	int pad;
	vec3 direction;
	int pad1;
	vec4 color;
};

layout (buffer_reference, scalar) readonly buffer SceneBuffer {
	mat4 view;
	mat4 proj;
	mat4 viewproj;
  	vec4 fog_color;
	vec3 camera_position;
	float ambient_light_factor;
	vec3 ambient_light_color;
  	float fog_end;
  	float fog_start;
    MaterialsBuffer materials;
	int number_of_lights;
} sceneBuffer;