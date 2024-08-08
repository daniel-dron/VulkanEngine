layout (buffer_reference, scalar) readonly buffer SceneBuffer {
	mat4 view;
	mat4 proj;
	mat4 viewproj;
  	vec4 fog_color;
  	float fog_end;
  	float fog_start;
	float _pad1;
	float _pad2;
	vec3 camera_position;
	float ambient_light_factor;
	vec3 ambient_light_color;
	int number_of_lights;
	PointLight pointLights[10];
} sceneBuffer;