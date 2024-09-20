#pragma once

#include <vk_types.h>
#include <graphics/gfx_device.h>
#include <vk_initializers.h>

template<typename PushConstantType>
class BindlessComputePipeline {
public:
	void init( GfxDevice& gfx, VkShaderModule compute_shader ) {
		VkPushConstantRange range = {
			.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
			.offset = 0,
			.size = sizeof( PushConstantType )
		};

		auto bindless_layout = gfx.getBindlessLayout( );

		// -----------
		// output layout
		VkDescriptorSetLayoutBinding output_binding = {};
		output_binding.binding = 0;
		output_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		output_binding.descriptorCount = 1;
		output_binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		output_binding.pImmutableSamplers = nullptr;

		VkDescriptorSetLayoutCreateInfo output_layout_create_info = {};
		output_layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		output_layout_create_info.bindingCount = 1;
		output_layout_create_info.pBindings = &output_binding;
		vkCreateDescriptorSetLayout( gfx.device, &output_layout_create_info, nullptr, &output_layout );

		// -----------
		// pipeline
		VkDescriptorSetLayout layouts[] = { bindless_layout, output_layout };

		VkPipelineLayoutCreateInfo layout_info = vkinit::pipeline_layout_create_info( );
		layout_info.pSetLayouts = layouts;
		layout_info.setLayoutCount = 2;
		layout_info.pPushConstantRanges = &range;
		layout_info.pushConstantRangeCount = 1;
		VK_CHECK( vkCreatePipelineLayout( gfx.device, &layout_info, nullptr, &layout ) );

		VkPipelineShaderStageCreateInfo stageinfo{};
		stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stageinfo.pNext = nullptr;
		stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
		stageinfo.module = compute_shader;
		stageinfo.pName = "main";

		VkComputePipelineCreateInfo create_info{};
		create_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		create_info.pNext = nullptr;
		create_info.layout = layout;
		create_info.stage = stageinfo;

		VK_CHECK( vkCreateComputePipelines( gfx.device, VK_NULL_HANDLE, 1, &create_info, nullptr, &pipeline ) );

		// -----------
		// create pool
		VkDescriptorPoolSize poolSize = {};
		poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		poolSize.descriptorCount = 1;

		VkDescriptorPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = 1;
		poolInfo.pPoolSizes = &poolSize;
		poolInfo.maxSets = 1;
		vkCreateDescriptorPool( gfx.device, &poolInfo, nullptr, &descriptor_pool );

		// create set
		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = descriptor_pool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &output_layout;
		vkAllocateDescriptorSets( gfx.device, &allocInfo, &output_set );
	}

	void cleanup( GfxDevice& gfx ) {
		vkDestroyDescriptorPool( gfx.device, descriptor_pool, nullptr );
		
		vkDestroyDescriptorSetLayout( gfx.device, output_layout, nullptr );
		vkDestroyPipelineLayout( gfx.device, layout, nullptr );
		vkDestroyPipeline( gfx.device, pipeline, nullptr );
	}

	VkPipelineLayout layout;
	VkPipeline pipeline;
	VkDescriptorSetLayout output_layout;

	VkDescriptorSet output_set;
	VkDescriptorPool descriptor_pool;
};
