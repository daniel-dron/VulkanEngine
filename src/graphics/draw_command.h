#pragma once

#include <vk_types.h>

struct MeshDrawCommand {
	VkBuffer indexBuffer;
	uint32_t indexCount;
	VkDeviceAddress vertexBufferAddress;

	Mat4 worldFromLocal;
	MaterialId materialId;
};
