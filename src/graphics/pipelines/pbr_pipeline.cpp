#include "pbr_pipeline.h"

#include <vk_pipelines.h>
#include <vk_initializers.h>

using namespace vkinit;
using namespace vkutil;

PbrPipeline::Result<> PbrPipeline::init( GfxDevice& gfx ) {

	VkShaderModule frag_shader;
	if ( !load_shader_module( "../../shaders/pbr.frag.spv", gfx.device, &frag_shader ) ) {
		return std::unexpected( PipelineError{
			.error = Error::ShaderLoadingFailed,
			.message = "Failed to load mesh fragment shader!"
			} );
	}

	VkShaderModule vert_shader;
	if ( !load_shader_module( "../../shaders/fullscreen_tri.vert.spv", gfx.device, &vert_shader ) ) {
		return std::unexpected( PipelineError{
			.error = Error::ShaderLoadingFailed,
			.message = "Failed to load mesh vertex shader!"
			} );
	}

	VkPushConstantRange range = {
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		.offset = 0,
		.size = sizeof( PushConstants )
	};

	auto bindless_layout = gfx.getBindlessLayout( );

	VkDescriptorSetLayout layouts[] = { bindless_layout };

	// ----------
	// pipeline
	VkPipelineLayoutCreateInfo layout_info = pipeline_layout_create_info( );
	layout_info.pSetLayouts = layouts;
	layout_info.setLayoutCount = 1;
	layout_info.pPushConstantRanges = &range;
	layout_info.pushConstantRangeCount = 1;
	VK_CHECK( vkCreatePipelineLayout( gfx.device, &layout_info, nullptr, &layout ) );

	PipelineBuilder builder;
	builder.set_shaders( vert_shader, frag_shader );
	builder.set_input_topology( VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST );
	builder.set_polygon_mode( VK_POLYGON_MODE_FILL );
	builder.set_cull_mode( VK_CULL_MODE_FRONT_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE );
	builder.set_multisampling_none( );
	builder.disable_blending( );
	builder.disable_depthtest( );

	auto& color = gfx.image_codex.getImage( gfx.swapchain.getCurrentFrame( ).color );
	auto& depth = gfx.image_codex.getImage( gfx.swapchain.getCurrentFrame( ).depth );
	builder.set_color_attachment_format( color.format );
	builder.set_depth_format( depth.format );
	builder._pipelineLayout = layout;
	pipeline = builder.build_pipeline( gfx.device );

#ifdef ENABLE_DEBUG_UTILS
	const VkDebugUtilsObjectNameInfoEXT obj = {
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
		.pNext = nullptr,
		.objectType = VkObjectType::VK_OBJECT_TYPE_PIPELINE,
		.objectHandle = (uint64_t)pipeline,
		.pObjectName = "PBR Pipeline"
	};
	vkSetDebugUtilsObjectNameEXT( gfx.device, &obj );
#endif

	vkDestroyShaderModule( gfx.device, frag_shader, nullptr );
	vkDestroyShaderModule( gfx.device, vert_shader, nullptr );
	gpu_scene_data = gfx.allocate( sizeof( GpuSceneData ), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VMA_MEMORY_USAGE_CPU_TO_GPU, "Scene Data PBR Pipeline" );

	return {};
}

void PbrPipeline::cleanup( GfxDevice& gfx ) {
	vkDestroyPipelineLayout( gfx.device, layout, nullptr );
	vkDestroyPipeline( gfx.device, pipeline, nullptr );
	gfx.free( gpu_scene_data );
}

DrawStats PbrPipeline::draw( GfxDevice& gfx, VkCommandBuffer cmd, const GpuSceneData& scene_data, const GBuffer& gbuffer ) const {
	DrawStats stats = {};

	GpuSceneData* gpu_scene_addr = nullptr;
	vmaMapMemory( gfx.allocator, gpu_scene_data.allocation, (void**)&gpu_scene_addr );
	*gpu_scene_addr = scene_data;
	gpu_scene_addr->materials = gfx.material_codex.getDeviceAddress( );
	vmaUnmapMemory( gfx.allocator, gpu_scene_data.allocation );

	vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline );

	auto bindless_set = gfx.getBindlessSet( );
	vkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &bindless_set, 0, nullptr );

	auto& target_image = gfx.image_codex.getImage( gfx.swapchain.getCurrentFrame( ).color );

	VkViewport viewport = {
		.x = 0,
		.y = 0,
		.width = static_cast<float>(target_image.extent.width),
		.height = static_cast<float>(target_image.extent.height),
		.minDepth = 0.0f,
		.maxDepth = 1.0f,
	};
	vkCmdSetViewport( cmd, 0, 1, &viewport );

	VkRect2D scissor = {
		.offset = {
			.x = 0,
			.y = 0
		},
		.extent = {
			.width = target_image.extent.width,
			.height = target_image.extent.height
		}
	};
	vkCmdSetScissor( cmd, 0, 1, &scissor );

	VkBufferDeviceAddressInfo address_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
		.pNext = nullptr,
		.buffer = gpu_scene_data.buffer
	};
	auto gpu_scene_address = vkGetBufferDeviceAddress( gfx.device, &address_info );

	PushConstants push_constants = {
		.scene_data_address = gpu_scene_address,
		.albedo_tex = gbuffer.albedo,
		.normal_tex = gbuffer.normal,
		.position_tex = gbuffer.position,
		.pbr_tex = gbuffer.pbr
	};
	vkCmdPushConstants( cmd, layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( PushConstants ), &push_constants );

	vkCmdDraw( cmd, 3, 1, 0, 0 );

	stats.drawcall_count++;
	stats.triangle_count += 1;

	return stats;
}