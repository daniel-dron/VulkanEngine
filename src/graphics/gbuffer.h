#pragma once

#include <vk_types.h>

struct GBuffer {
	ImageID albedo;
	ImageID normal;
	ImageID position;
	ImageID pbr;
};

struct ImGuiGBuffer {
	VkDescriptorSet albedo_set = nullptr;
	VkDescriptorSet normal_set = nullptr;
	VkDescriptorSet position_set = nullptr;
	VkDescriptorSet pbr_set = nullptr;
};