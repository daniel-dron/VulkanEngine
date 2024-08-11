#pragma once

#include <vk_types.h>

struct MeshDrawCommand {
	VkBuffer index_buffer;
	uint32_t index_count;
	VkDeviceAddress vertex_buffer_address;

	mat4 world_from_local;
	MaterialID material_id;
};
