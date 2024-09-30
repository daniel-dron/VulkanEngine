#include "wireframe_pipeline.h"

#include <vk_pipelines.h>
#include <vk_initializers.h>

using namespace vkinit;
using namespace vkutil;

WireframePipeline::Result<> WireframePipeline::init( GfxDevice& gfx ) {
	auto& frag_shader = gfx.shader_storage->Get("wireframe", T_FRAGMENT);
	auto& vert_shader = gfx.shader_storage->Get("wireframe", T_VERTEX);

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
	builder.set_shaders( vert_shader.handle, frag_shader.handle );
	builder.set_input_topology( VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST );
	builder.set_polygon_mode( VK_POLYGON_MODE_LINE );
	builder.set_cull_mode( VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE );
	builder.set_multisampling_none( );
	builder.disable_blending( );
	builder.enable_depthtest( true, VK_COMPARE_OP_GREATER_OR_EQUAL );

	auto& color = gfx.image_codex.getImage( gfx.swapchain.getCurrentFrame( ).hdr_color );
	auto& depth = gfx.image_codex.getImage( gfx.swapchain.getCurrentFrame( ).depth );
	builder.set_color_attachment_format( color.GetFormat( ) );
	builder.set_depth_format( depth.GetFormat( ) );
	builder._pipelineLayout = layout;
	pipeline = builder.build_pipeline( gfx.device );

#ifdef ENABLE_DEBUG_UTILS
	const VkDebugUtilsObjectNameInfoEXT obj = {
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
		.pNext = nullptr,
		.objectType = VkObjectType::VK_OBJECT_TYPE_PIPELINE,
		.objectHandle = (uint64_t)pipeline,
		.pObjectName = "Wireframe Pipeline"
	};
	vkSetDebugUtilsObjectNameEXT( gfx.device, &obj );
#endif

	gpu_scene_data = gfx.allocate( sizeof( GpuSceneData ), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, "Scene Data Mesh Pipeline" );

	return {};
}

void WireframePipeline::cleanup( GfxDevice& gfx ) {
	vkDestroyPipelineLayout( gfx.device, layout, nullptr );
	vkDestroyPipeline( gfx.device, pipeline, nullptr );
	gfx.free( gpu_scene_data );
}

DrawStats WireframePipeline::draw( GfxDevice& gfx, VkCommandBuffer cmd, const std::vector<MeshDrawCommand>& draw_commands, const GpuSceneData& scene_data ) const {
	DrawStats stats = {};

	GpuSceneData* gpu_scene_addr = nullptr;
	vmaMapMemory( gfx.allocator, gpu_scene_data.allocation, (void**)&gpu_scene_addr );
	*gpu_scene_addr = scene_data;
	vmaUnmapMemory( gfx.allocator, gpu_scene_data.allocation );

	vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline );

	auto bindless_set = gfx.getBindlessSet( );
	vkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &bindless_set, 0, nullptr );

	auto& target_image = gfx.image_codex.getImage( gfx.swapchain.getCurrentFrame( ).hdr_color );

	VkViewport viewport = {
		.x = 0,
		.y = 0,
		.width = static_cast<float>(target_image.GetExtent( ).width),
		.height = static_cast<float>(target_image.GetExtent( ).height),
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
			.width = target_image.GetExtent( ).width,
			.height = target_image.GetExtent( ).height
		}
	};
	vkCmdSetScissor( cmd, 0, 1, &scissor );

	for ( const auto& draw_command : draw_commands ) {
		vkCmdBindIndexBuffer( cmd, draw_command.index_buffer, 0, VK_INDEX_TYPE_UINT32 );

		VkBufferDeviceAddressInfo address_info = {
			.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
			.pNext = nullptr,
			.buffer = gpu_scene_data.buffer
		};
		auto gpu_scene_address = vkGetBufferDeviceAddress( gfx.device, &address_info );

		PushConstants push_constants = {
			.world_from_local = draw_command.world_from_local,
			.scene_data_address = gpu_scene_address,
			.vertex_buffer_address = draw_command.vertex_buffer_address
		};
		vkCmdPushConstants( cmd, layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( PushConstants ), &push_constants );

		vkCmdDrawIndexed( cmd, draw_command.index_count, 1, 0, 0, 0 );

		stats.drawcall_count++;
		stats.triangle_count += draw_command.index_count / 3;
	}

	return stats;
}
