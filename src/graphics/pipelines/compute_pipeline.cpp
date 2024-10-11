#include <pch.h>

#include "compute_pipeline.h"

void BindlessCompute::addDescriptorSetLayout( uint32_t binding, VkDescriptorType type ) {
	layout_builder.AddBinding( binding, type );
}

void BindlessCompute::addPushConstantRange( uint32_t size ) {
	VkPushConstantRange range = {};
	range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	range.offset = 0;
	range.size = size;
	push_constant_ranges.push_back( range );
}

void BindlessCompute::build( GfxDevice& gfx, VkShaderModule shader, const std::string& name ) {
	auto bindless_layout = gfx.getBindlessLayout( );

	descriptor_layout = layout_builder.Build( gfx.device, VK_SHADER_STAGE_COMPUTE_BIT, nullptr );

	VkDescriptorSetLayout layouts[] = { bindless_layout, descriptor_layout };

	VkPipelineLayoutCreateInfo layout_info = vkinit::pipeline_layout_create_info( );
	layout_info.pSetLayouts = layouts;
	layout_info.setLayoutCount = 2;
	layout_info.pPushConstantRanges = push_constant_ranges.data( );
	layout_info.pushConstantRangeCount = push_constant_ranges.size( );
	VK_CHECK( vkCreatePipelineLayout( gfx.device, &layout_info, nullptr, &layout ) );

	VkPipelineShaderStageCreateInfo stageinfo{};
	stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stageinfo.pNext = nullptr;
	stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stageinfo.module = shader;
	stageinfo.pName = "main";

	VkComputePipelineCreateInfo create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	create_info.pNext = nullptr;
	create_info.layout = layout;
	create_info.stage = stageinfo;

	VK_CHECK( vkCreateComputePipelines( gfx.device, VK_NULL_HANDLE, 1, &create_info, nullptr, &pipeline ) );

#ifdef ENABLE_DEBUG_UTILS
	const VkDebugUtilsObjectNameInfoEXT obj = {
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
		.pNext = nullptr,
		.objectType = VkObjectType::VK_OBJECT_TYPE_PIPELINE,
		.objectHandle = (uint64_t)pipeline,
		.pObjectName = name.c_str()
	};
	vkSetDebugUtilsObjectNameEXT( gfx.device, &obj );
#endif
}

void BindlessCompute::cleanup( GfxDevice& gfx ) {
	if ( pipeline != VK_NULL_HANDLE ) {
		vkDestroyPipeline( gfx.device, pipeline, nullptr );
	}
	if ( layout != VK_NULL_HANDLE ) {
		vkDestroyPipelineLayout( gfx.device, layout, nullptr );
	}

	if ( layout != VK_NULL_HANDLE ) {
		vkDestroyDescriptorSetLayout( gfx.device, descriptor_layout, nullptr );
	}
}


void BindlessCompute::bind( VkCommandBuffer cmd ) const {
	vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline );
}

void BindlessCompute::bindDescriptorSet( VkCommandBuffer cmd, VkDescriptorSet set, uint32_t index ) const {
	vkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, layout, index, 1, &set, 0, nullptr );
}

void BindlessCompute::pushConstants( VkCommandBuffer cmd, uint32_t size, const void* value ) const {
	vkCmdPushConstants( cmd, layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, size, value );
}

void BindlessCompute::dispatch( VkCommandBuffer cmd, uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z ) const {
	vkCmdDispatch( cmd, group_count_x, group_count_y, group_count_z );
}
