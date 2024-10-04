#include "shadowmap.h"
#include <vk_initializers.h>
#include <vk_pipelines.h>

using namespace vkinit;
using namespace vkutil;

ShadowMap::Result<> ShadowMap::init( GfxDevice& gfx ) {
	auto& frag_shader = gfx.shader_storage->Get( "shadowmap", T_FRAGMENT );
	auto& vert_shader = gfx.shader_storage->Get( "shadowmap", T_VERTEX );

	auto ReconstructShaderCallback = [&]( VkShaderModule shader ) {
		VK_CHECK( vkWaitForFences( gfx.device, 1, &gfx.swapchain.getCurrentFrame( ).fence, true, 1000000000 ) );
		cleanup( gfx );

		Reconstruct( gfx );
	};

	frag_shader.RegisterReloadCallback( ReconstructShaderCallback );
	vert_shader.RegisterReloadCallback( ReconstructShaderCallback );

	Reconstruct( gfx );

	return {};
}

void ShadowMap::cleanup( GfxDevice& gfx ) {
	vkDestroyPipelineLayout( gfx.device, layout, nullptr );
	vkDestroyPipeline( gfx.device, pipeline, NULL );
}

DrawStats ShadowMap::draw( GfxDevice& gfx, VkCommandBuffer cmd, const std::vector<MeshDrawCommand>& draw_commands, const glm::mat4& projection, const glm::mat4 view, ImageID target ) const {
	DrawStats stats = {};

	vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline );

	auto& target_image = gfx.image_codex.getImage( target );

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

		PushConstants push_constants = {
			.projection = projection,
			.view = view,
			.model = draw_command.world_from_local,
			.vertex_buffer_address = draw_command.vertex_buffer_address
		};
		vkCmdPushConstants( cmd, layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( PushConstants ), &push_constants );

		vkCmdDrawIndexed( cmd, draw_command.index_count, 1, 0, 0, 0 );

		stats.drawcall_count++;
		stats.triangle_count += draw_command.index_count / 3;
	}

	return stats;

}

void ShadowMap::Reconstruct( GfxDevice& gfx ) {
	auto& frag_shader = gfx.shader_storage->Get( "shadowmap", T_FRAGMENT );
	auto& vert_shader = gfx.shader_storage->Get( "shadowmap", T_VERTEX );

	VkPushConstantRange range = {
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		.offset = 0,
		.size = sizeof( PushConstants )
	};

	VkPipelineLayoutCreateInfo layout_info = pipeline_layout_create_info( );
	layout_info.pSetLayouts = nullptr;
	layout_info.setLayoutCount = 0;
	layout_info.pPushConstantRanges = &range;
	layout_info.pushConstantRangeCount = 1;
	VK_CHECK( vkCreatePipelineLayout( gfx.device, &layout_info, nullptr, &layout ) );

	PipelineBuilder builder;
	builder.set_shaders( vert_shader.handle, frag_shader.handle );
	builder.set_input_topology( VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST );
	builder.set_polygon_mode( VK_POLYGON_MODE_FILL );
	builder.set_cull_mode( VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE );
	builder.set_multisampling_none( );
	builder.disable_blending( );
	builder.enable_depthtest( true, VK_COMPARE_OP_LESS );

	VkFormat format = gfx.image_codex.getImage( gfx.swapchain.getCurrentFrame( ).depth ).GetFormat( );
	builder.set_depth_format( format );

	builder._pipelineLayout = layout;
	pipeline = builder.build_pipeline( gfx.device );

#ifdef ENABLE_DEBUG_UTILS
	const VkDebugUtilsObjectNameInfoEXT obj = {
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
		.pNext = nullptr,
		.objectType = VkObjectType::VK_OBJECT_TYPE_PIPELINE,
		.objectHandle = (uint64_t)pipeline,
		.pObjectName = "ShadowMap Pipeline"
	};
	vkSetDebugUtilsObjectNameEXT( gfx.device, &obj );
#endif
}
