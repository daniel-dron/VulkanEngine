#include "bindless.h"
#include <graphics/gfx_device.h>

void BindlessRegistry::init( GfxDevice& gfx ) {
	// ----------
	// Create descriptor pool
	{
		VkDescriptorPoolSize pool_sizes[] = {
			{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, MAX_BINDLESS_IMAGES},
			{VK_DESCRIPTOR_TYPE_SAMPLER, MAX_SAMPLERS}
		};

		VkDescriptorPoolCreateInfo info = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.pNext = nullptr,
			.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
			.maxSets = MAX_BINDLESS_IMAGES * 2,
			.poolSizeCount = 2u,
			.pPoolSizes = pool_sizes
		};

		VK_CHECK( vkCreateDescriptorPool( gfx.device, &info, nullptr, &pool ) );
	}

	// ----------
	// Descriptor Set Layout
	{
		VkDescriptorSetLayoutBinding bindings[] = {
			{
				.binding = TEXTURE_BINDING,
				.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
				.descriptorCount = MAX_BINDLESS_IMAGES,
				.stageFlags = VK_SHADER_STAGE_ALL
			},
			{
				.binding = SAMPLERS_BINDING,
				.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
				.descriptorCount = MAX_SAMPLERS,
				.stageFlags = VK_SHADER_STAGE_ALL
			},
		};

		VkDescriptorBindingFlags binding_flags[2] = {
			VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
			VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
		};

		VkDescriptorSetLayoutBindingFlagsCreateInfo flag_info = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
			.pNext = nullptr,
			.pBindingFlags = binding_flags
		};

		VkDescriptorSetLayoutCreateInfo info = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.pNext = &flag_info,
			.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT,
			.bindingCount = 2u,
			.pBindings = bindings
		};

		VK_CHECK( vkCreateDescriptorSetLayout( gfx.device, &info, nullptr, &layout ) );
	}

	// ---------
	// Allocate descriptor set
	{
		VkDescriptorSetAllocateInfo alloc_info = {
		   .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		   .descriptorPool = pool,
		   .descriptorSetCount = 1,
		   .pSetLayouts = &layout,
		};

		std::uint32_t max_binding = MAX_BINDLESS_IMAGES - 1;
		const auto countInfo = VkDescriptorSetVariableDescriptorCountAllocateInfo{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
			.descriptorSetCount = 1,
			.pDescriptorCounts = &max_binding,
		};

		VK_CHECK( vkAllocateDescriptorSets( gfx.device, &alloc_info, &set ) );
	}

	initSamplers( gfx );
}

void BindlessRegistry::cleanup( GfxDevice& gfx ) {
	vkDestroySampler( gfx.device, nearest_sampler, nullptr );
	vkDestroySampler( gfx.device, linear_sampler, nullptr );
	vkDestroySampler( gfx.device, shadow_map_sampler, nullptr );

	vkDestroyDescriptorSetLayout( gfx.device, layout, nullptr );
	vkDestroyDescriptorPool( gfx.device, pool, nullptr );
}

void BindlessRegistry::addImage( GfxDevice& gfx, ImageID id, const VkImageView view ) {
	VkDescriptorImageInfo image_info = {
		.imageView = view,
		.imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL
	};

	VkWriteDescriptorSet write_set = {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = set,
		.dstBinding = TEXTURE_BINDING,
		.dstArrayElement = id,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
		.pImageInfo = &image_info,
	};
	vkUpdateDescriptorSets( gfx.device, 1, &write_set, 0, nullptr );
}

void BindlessRegistry::addSampler( GfxDevice& gfx, std::uint32_t id, const VkSampler sampler ) {
	VkDescriptorImageInfo info = { .sampler = sampler, .imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL };
	VkWriteDescriptorSet write_set = {
	   .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
	   .dstSet = set,
	   .dstBinding = SAMPLERS_BINDING,
	   .dstArrayElement = id,
	   .descriptorCount = 1,
	   .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
	   .pImageInfo = &info,
	};
	vkUpdateDescriptorSets( gfx.device, 1, &write_set, 0, nullptr );
}

void BindlessRegistry::initSamplers( GfxDevice& gfx ) {
	static const std::uint32_t nearest_sampler_id = 0;
	static const std::uint32_t linear_sampler_id = 1;
	static const std::uint32_t shadow_sampler_id = 2;

	{
		VkSamplerCreateInfo create_info = {
			.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
			.magFilter = VK_FILTER_NEAREST,
			.minFilter = VK_FILTER_NEAREST,
		};
		VK_CHECK( vkCreateSampler( gfx.device, &create_info, nullptr, &nearest_sampler ) );
		addSampler( gfx, nearest_sampler_id, nearest_sampler );
	}

	{
		VkSamplerCreateInfo create_info = {
			.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
			.magFilter = VK_FILTER_LINEAR,
			.minFilter = VK_FILTER_LINEAR,
			.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
		};
		VK_CHECK( vkCreateSampler( gfx.device, &create_info, nullptr, &linear_sampler ) );
		addSampler( gfx, linear_sampler_id, linear_sampler );
	}

	{
		VkSamplerCreateInfo create_info = {
			.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
			.magFilter = VK_FILTER_LINEAR,
			.minFilter = VK_FILTER_LINEAR,
			.compareEnable = VK_TRUE,
			.compareOp = VK_COMPARE_OP_GREATER_OR_EQUAL,
		};
		VK_CHECK( vkCreateSampler( gfx.device, &create_info, nullptr, &shadow_map_sampler ) );
		addSampler( gfx, shadow_sampler_id, shadow_map_sampler );
	}
}