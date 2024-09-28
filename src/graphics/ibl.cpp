#include "ibl.h"

#include <graphics/gfx_device.h>
#include <graphics/image_codex.h>

#include <vk_pipelines.h>
#include <graphics/pipelines/compute_pipeline.h>

void IBL::init( GfxDevice& gfx, const std::string& path ) {
	hdr_texture = gfx.image_codex.loadHDRFromFile( path, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT, false );

	VkCommandBufferAllocateInfo cmd_alloc_info = vkinit::command_buffer_allocate_info( gfx.compute_command_pool, 1 );
	VK_CHECK( vkAllocateCommandBuffers( gfx.device, &cmd_alloc_info, &compute_command ) );

	auto fence_create_info = vkinit::fence_create_info( );
	VK_CHECK( vkCreateFence( gfx.device, &fence_create_info, nullptr, &compute_fence ) );

	initTextures( gfx );

	initComputes( gfx );

	fmt::println( "Dispatching IBL computes!" );
	// dispatch computes
	{
		VK_CHECK( vkResetCommandBuffer( compute_command, 0 ) );
		auto cmd_begin_info = vkinit::command_buffer_begin_info( VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT );
		VK_CHECK( vkBeginCommandBuffer( compute_command, &cmd_begin_info ) );

		generateSkybox( gfx, compute_command );
		generateIrradiance( gfx, compute_command );
		generateRadiance( gfx, compute_command );
		generateBrdf( gfx, compute_command );

		VK_CHECK( vkEndCommandBuffer( compute_command ) );
		auto cmd_info = vkinit::command_buffer_submit_info( compute_command );
		auto cmd_submit_info = vkinit::submit_info( &cmd_info, nullptr, nullptr );
		VK_CHECK( vkQueueSubmit2( gfx.compute_queue, 1, &cmd_submit_info, compute_fence ) );
	}
}

void IBL::clean( GfxDevice& gfx ) {
	vkFreeCommandBuffers( gfx.device, gfx.compute_command_pool, 1, &compute_command );
	vkDestroyFence( gfx.device, compute_fence, nullptr );

	equirectangular_pipeline.cleanup( gfx );
	irradiance_pipeline.cleanup( gfx );
	radiance_pipeline.cleanup( gfx );
	brdf_pipeline.cleanup( gfx );
}

void IBL::initComputes( GfxDevice& gfx ) {
	VkShaderModule equi_map, irradiance_shader, radiance_shader, brdf_shader;
	vkutil::load_shader_module( "../../shaders/equirectangular_map.comp.spv", gfx.device, &equi_map );
	vkutil::load_shader_module( "../../shaders/irradiance.comp.spv", gfx.device, &irradiance_shader );
	vkutil::load_shader_module( "../../shaders/radiance.comp.spv", gfx.device, &radiance_shader );
	vkutil::load_shader_module( "../../shaders/brdf.comp.spv", gfx.device, &brdf_shader );

	// ----------
	// Equirectangular to Cubemap
	{
		equirectangular_pipeline.addDescriptorSetLayout( 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE );
		equirectangular_pipeline.addPushConstantRange( sizeof( ImageID ) );
		equirectangular_pipeline.build( gfx, equi_map, "Equirectangular to Cubemap Pipeline" );
		equirectangular_pipeline.createDescriptorPool( gfx, { {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1} }, 1 );
		equi_set = equirectangular_pipeline.allocateDescriptorSet( gfx );

		DescriptorWriter writer;
		auto& skybox_image = gfx.image_codex.getImage( skybox );
		writer.write_image( 0, skybox_image.GetBaseView( ), nullptr, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE );
		writer.update_set( gfx.device, equi_set );
	}

	// ----------
	// Irradiance
	{
		irradiance_pipeline.addDescriptorSetLayout( 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE );
		irradiance_pipeline.addPushConstantRange( sizeof( ImageID ) );
		irradiance_pipeline.build( gfx, irradiance_shader, "Irradiance Compute" );
		irradiance_pipeline.createDescriptorPool( gfx, { {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1} }, 1 );
		irradiance_set = irradiance_pipeline.allocateDescriptorSet( gfx );

		DescriptorWriter writer;
		auto& irradiance_image = gfx.image_codex.getImage( irradiance );
		writer.write_image( 0, irradiance_image.GetBaseView( ), nullptr, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE );
		writer.update_set( gfx.device, irradiance_set );
	}

	// ----------
	// Radiance
	{
		radiance_pipeline.addDescriptorSetLayout( 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE );
		radiance_pipeline.addPushConstantRange( sizeof( RadiancePushConstants ) );
		radiance_pipeline.build( gfx, radiance_shader, "Radiance Compute" );
		radiance_pipeline.createDescriptorPool( gfx, { {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1} }, 6 );

		auto& radiance_image = gfx.image_codex.getImage( radiance );

		for ( auto i = 0; i < 6; i++ ) {
			radiance_sets[i] = radiance_pipeline.allocateDescriptorSet( gfx );

			DescriptorWriter writer;
			writer.write_image( 0, radiance_image.GetMipView( i ), nullptr, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE );
			writer.update_set( gfx.device, radiance_sets[i] );
		}
	}

	// ----------
	// BRDF
	{
		brdf_pipeline.addDescriptorSetLayout( 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE );
		brdf_pipeline.build( gfx, brdf_shader, "BRDF Compute" );
		brdf_pipeline.createDescriptorPool( gfx, { {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1} }, 1 );
		brdf_set = brdf_pipeline.allocateDescriptorSet( gfx );

		DescriptorWriter writer;
		auto& brdf_image = gfx.image_codex.getImage( brdf );
		writer.write_image( 0, brdf_image.GetBaseView( ), nullptr, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE );
		writer.update_set( gfx.device, brdf_set );
	}


	vkDestroyShaderModule( gfx.device, equi_map, nullptr );
	vkDestroyShaderModule( gfx.device, irradiance_shader, nullptr );
	vkDestroyShaderModule( gfx.device, radiance_shader, nullptr );
	vkDestroyShaderModule( gfx.device, brdf_shader, nullptr );
}

void IBL::initTextures( GfxDevice& gfx ) {
	VkImageUsageFlags usages{};
	usages |= VK_IMAGE_USAGE_SAMPLED_BIT;
	usages |= VK_IMAGE_USAGE_STORAGE_BIT;

	skybox = gfx.image_codex.createCubemap( "Skybox", VkExtent3D{ 2048, 2048, 1 }, VK_FORMAT_R32G32B32A32_SFLOAT, usages );
	irradiance = gfx.image_codex.createCubemap( "Irradiance", VkExtent3D{ 32, 32, 1 }, VK_FORMAT_R32G32B32A32_SFLOAT, usages );
	radiance = gfx.image_codex.createCubemap( "Radiance", VkExtent3D{ 128, 128, 1 }, VK_FORMAT_R32G32B32A32_SFLOAT, usages, 6 );
	brdf = gfx.image_codex.createEmptyImage( "BRDF", VkExtent3D{ 512, 512, 1 }, VK_FORMAT_R32G32B32A32_SFLOAT, usages );
}

void IBL::generateSkybox( GfxDevice& gfx, VkCommandBuffer cmd ) const {
	auto bindless = gfx.getBindlessSet( );
	auto input = hdr_texture;
	auto output = skybox;

	auto& output_image = gfx.image_codex.getImage( output );

	equirectangular_pipeline.bind( cmd );
	equirectangular_pipeline.bindDescriptorSet( cmd, bindless, 0 );
	equirectangular_pipeline.bindDescriptorSet( cmd, equi_set, 1 );
	equirectangular_pipeline.pushConstants( cmd, sizeof( ImageID ), &input );
	equirectangular_pipeline.dispatch( cmd, (output_image.GetExtent( ).width + 15) / 16, (output_image.GetExtent( ).height + 15) / 16, 6 );
}

void IBL::generateIrradiance( GfxDevice& gfx, VkCommandBuffer cmd ) const {
	auto bindless = gfx.getBindlessSet( );
	auto input = skybox;
	auto output = irradiance;

	auto& output_image = gfx.image_codex.getImage( output );
	irradiance_pipeline.bind( cmd );
	irradiance_pipeline.bindDescriptorSet( cmd, bindless, 0 );
	irradiance_pipeline.bindDescriptorSet( cmd, irradiance_set, 1 );
	irradiance_pipeline.pushConstants( cmd, sizeof( ImageID ), &input );
	irradiance_pipeline.dispatch( cmd, (output_image.GetExtent( ).width + 15) / 16, (output_image.GetExtent( ).height + 15) / 16, 6 );
}

void IBL::generateRadiance( GfxDevice& gfx, VkCommandBuffer cmd ) const {
	auto bindless = gfx.getBindlessSet( );
	auto input = skybox;
	auto output = radiance;

	auto& output_image = gfx.image_codex.getImage( output );

	RadiancePushConstants pc = {
		.input = input,
		.mipmap = 0,
		.roughness = 0
	};

	radiance_pipeline.bind( cmd );
	radiance_pipeline.bindDescriptorSet( cmd, bindless, 0 );

	for ( auto mip = 0; mip < 6; mip++ ) {
		uint32_t mipSize = output_image.GetExtent( ).width >> mip;
		float roughness = (float)mip / (float)(6 - 1);

		pc.mipmap = mip;
		pc.roughness = roughness;

		auto set = radiance_sets[mip];

		radiance_pipeline.bindDescriptorSet( cmd, set, 1 );
		radiance_pipeline.pushConstants( cmd, sizeof( RadiancePushConstants ), &pc );
		radiance_pipeline.dispatch( cmd, (output_image.GetExtent( ).width + 15) / 16, (output_image.GetExtent( ).height + 15) / 16, 6 );
	}
}

void IBL::generateBrdf( GfxDevice& gfx, VkCommandBuffer cmd ) const {
	auto bindless = gfx.getBindlessSet( );
	auto output = brdf;
	auto& output_image = gfx.image_codex.getImage( output );

	brdf_pipeline.bind( cmd );
	brdf_pipeline.bindDescriptorSet( cmd, bindless, 0 );
	brdf_pipeline.bindDescriptorSet( cmd, brdf_set, 1 );
	brdf_pipeline.dispatch( cmd, (output_image.GetExtent( ).width + 15) / 16, (output_image.GetExtent( ).height + 15) / 16, 1 );
}
