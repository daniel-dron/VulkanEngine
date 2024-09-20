#include "ibl.h"

#include <graphics/gfx_device.h>
#include <graphics/image_codex.h>

#include <vk_pipelines.h>
#include <graphics/pipelines/compute_pipeline.h>

void IBL::init( GfxDevice& gfx, VkCommandBuffer cmd, const std::string& path ) {
	hdr_texture = gfx.image_codex.loadHDRFromFile( path, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT, false );

	initComputes( gfx );

	initTextures( gfx );

	generateSkybox( gfx, cmd );
}

void IBL::clean( GfxDevice& gfx ) {
	equi_pipeline.cleanup( gfx );
}

void IBL::initComputes( GfxDevice& gfx ) {
	VkShaderModule equi_map;
	vkutil::load_shader_module( "../../shaders/equirectangular_map.comp.spv", gfx.device, &equi_map );

	// Equirectangular to Cubemap
	equi_pipeline = BindlessComputePipeline<ImageID>( );
	equi_pipeline.init( gfx, equi_map );

	vkDestroyShaderModule( gfx.device, equi_map, nullptr );
}

void IBL::initTextures( GfxDevice& gfx ) {
	VkImageUsageFlags usages{};
	usages |= VK_IMAGE_USAGE_SAMPLED_BIT;
	usages |= VK_IMAGE_USAGE_STORAGE_BIT;

	skybox = gfx.image_codex.createCubemap( "Skybox", VkExtent3D{ 1024, 1024, 1 }, VK_FORMAT_R32G32B32A32_SFLOAT, usages );
	irradiance = gfx.image_codex.createCubemap( "Irradiance", VkExtent3D{ 32, 32, 1 }, VK_FORMAT_R32G32B32A32_SFLOAT, usages );
	brdf = gfx.image_codex.createEmptyImage( "BRDF", VkExtent3D{ 32, 32, 1 }, VK_FORMAT_R32G32B32A32_SFLOAT, usages );
}

void IBL::generateSkybox( GfxDevice& gfx, VkCommandBuffer cmd ) const {
	auto bindless = gfx.getBindlessSet( );
	auto input = hdr_texture;
	auto output = skybox;

	auto& output_image = gfx.image_codex.getImage( output );

	// write to set
	VkDescriptorImageInfo output_info = {};
	output_info.imageView = output_image.view;
	output_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	VkWriteDescriptorSet write_set = {};
	write_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write_set.dstSet = equi_pipeline.output_set;
	write_set.dstBinding = 0;
	write_set.dstArrayElement = 0;
	write_set.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	write_set.descriptorCount = 1;
	write_set.pImageInfo = &output_info;
	vkUpdateDescriptorSets( gfx.device, 1, &write_set, 0, nullptr );

	vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, equi_pipeline.pipeline );
	vkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, equi_pipeline.layout, 0, 1, &bindless, 0, nullptr );
	vkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, equi_pipeline.layout, 1, 1, &equi_pipeline.output_set, 0, nullptr );
	vkCmdPushConstants( cmd, equi_pipeline.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( ImageID ), &input );
	vkCmdDispatch( cmd, (output_image.extent.width + 15) / 16, (output_image.extent.height + 15) / 16, 6 );
}

//void go( GfxDevice& gfx, VkCommandBuffer cmd, ImageID input ) {
//	// create pool
//	VkDescriptorPoolSize poolSize = {};
//	poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
//	poolSize.descriptorCount = 1;
//
//	VkDescriptorPoolCreateInfo poolInfo = {};
//	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
//	poolInfo.poolSizeCount = 1;
//	poolInfo.pPoolSizes = &poolSize;
//	poolInfo.maxSets = 1;
//
//	VkDescriptorPool descriptorPool;
//	vkCreateDescriptorPool( gfx.device, &poolInfo, nullptr, &descriptorPool );
//
//	// create set
//	VkDescriptorSetAllocateInfo allocInfo = {};
//	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
//	allocInfo.descriptorPool = descriptorPool;
//	allocInfo.descriptorSetCount = 1;
//	allocInfo.pSetLayouts = &pipeline.output_layout;
//
//	VkDescriptorSet output_set;
//	vkAllocateDescriptorSets( gfx.device, &allocInfo, &output_set );
//
//	// write to set
//	VkDescriptorImageInfo outputImageInfo = {};
//	outputImageInfo.imageView = gfx.image_codex.getImage( irradiance ).view;
//	outputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
//
//	VkWriteDescriptorSet writeDescriptorSet = {};
//	writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
//	writeDescriptorSet.dstSet = output_set;
//	writeDescriptorSet.dstBinding = 0;
//	writeDescriptorSet.dstArrayElement = 0;
//	writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
//	writeDescriptorSet.descriptorCount = 1;
//	writeDescriptorSet.pImageInfo = &outputImageInfo;
//
//	vkUpdateDescriptorSets( gfx.device, 1, &writeDescriptorSet, 0, nullptr );
//
//	// render
//	auto bindless = gfx.getBindlessSet( );
//
//	TestPushConstant pc = {
//		.input = skybox
//	};
//
//	vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.pipeline );
//	vkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.layout, 0, 1, &bindless, 0, nullptr );
//	vkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.layout, 1, 1, &output_set, 0, nullptr );
//	vkCmdPushConstants( cmd, pipeline.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( TestPushConstant ), &pc );
//	vkCmdDispatch( cmd, 32 / 16, 32 / 16, 6 );
//}
