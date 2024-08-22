#pragma once

#include <vk_types.h>

struct GpuImage {
	VkImage image;
	VkImageView view;
	VmaAllocation allocation;
	VkExtent3D extent;
	VkFormat format;
	VkImageUsageFlags usage;
	ImageID id;
	bool cubemap = false;
	bool mipmapped;

	struct Info {
		std::string path;
		std::string debug_name;
	} info;
};