#extension GL_EXT_buffer_reference : require

struct Material {
    vec4 base_color;
	vec4 metal_roughness_factors;
	uint color_tex;
	uint metal_roughness_tex;
	uint normal_tex;
	uint pad;
};

layout (buffer_reference, scalar) readonly buffer MaterialsBuffer {
    Material mat[];
} materialsBuffer;