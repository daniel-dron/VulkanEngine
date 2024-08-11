#pragma once

#include <vk_types.h>

struct OldMeshDrawCommand {
	uint32_t index_count;
	uint32_t first_index;
	VkBuffer index_buffer;

	MaterialInstance* material;
	Bounds bounds;
	
	mat4 transform;
	VkDeviceAddress vertex_buffer_address;
};

struct MeshDrawCommand {
	VkBuffer index_buffer;
	uint32_t index_count;
	VkDeviceAddress vertex_buffer_address;

	mat4 world_from_local;
	MaterialID material_id;
};
