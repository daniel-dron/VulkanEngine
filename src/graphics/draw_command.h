#pragma once

#include <vk_types.h>

struct MeshDrawCommand {
	uint32_t index_count;
	uint32_t first_index;
	VkBuffer index_buffer;

	MaterialInstance* material;
	Bounds bounds;
	
	mat4 transform;
	VkDeviceAddress vertex_buffer_address;
};