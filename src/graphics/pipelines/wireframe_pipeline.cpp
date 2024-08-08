#include "wireframe_pipeline.h"

#include <vk_pipelines.h>
#include <vk_initializers.h>

using namespace vkinit;
using namespace vkutil;

WireframePipeline::Result<> WireframePipeline::init( GfxDevice& gfx ) {
	VkShaderModule frag_shader;
	if ( !load_shader_module( "../../shaders/wireframe.frag.spv", gfx.device, &frag_shader ) ) {
		return std::unexpected( PipelineError{
			.error = Error::ShaderLoadingFailed,
			.message = "Failed to load mesh fragment shader!"
			} );
	}

	VkShaderModule vert_shader;
	if ( !load_shader_module( "../../shaders/mesh.vert.spv", gfx.device, &vert_shader ) ) {
		return std::unexpected( PipelineError{
			.error = Error::ShaderLoadingFailed,
			.message = "Failed to load mesh vertex shader!"
			} );
	}

	// ----------
	// layouts
	{
		DescriptorLayoutBuilder builder;
		builder.add_binding( 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER );
		scene_data_layout = builder.build( gfx.device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr );
	}

	VkPushConstantRange range = {
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
		.offset = 0,
		.size = sizeof( PushConstants )
	};

	DescriptorLayoutBuilder layout_builder;
	layout_builder.add_binding( 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER );
	layout_builder.add_binding( 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER );
	layout_builder.add_binding( 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER );
	layout_builder.add_binding( 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER );
	material_layout = layout_builder.build( gfx.device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr );

	auto bindless_layout = gfx.getBindlessLayout( );
	VkDescriptorSetLayout layouts[] = {
		bindless_layout, scene_data_layout, material_layout
	};

	// ----------
	// pipeline
	VkPipelineLayoutCreateInfo layout_info = pipeline_layout_create_info( );
	layout_info.pSetLayouts = layouts;
	layout_info.setLayoutCount = 3;
	layout_info.pPushConstantRanges = &range;
	layout_info.pushConstantRangeCount = 1;
	VK_CHECK( vkCreatePipelineLayout( gfx.device, &layout_info, nullptr, &layout ) );

	PipelineBuilder builder;
	builder.set_shaders( vert_shader, frag_shader );
	builder.set_input_topology( VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST );
	builder.set_polygon_mode( VK_POLYGON_MODE_LINE );
	builder.set_cull_mode( VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE );
	builder.set_multisampling_none( );
	builder.disable_blending( );
	builder.enable_depthtest( true, VK_COMPARE_OP_GREATER_OR_EQUAL );

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
		.pObjectName = "Wireframe Pipeline"
	};
	vkSetDebugUtilsObjectNameEXT( gfx.device, &obj );
#endif

	vkDestroyShaderModule( gfx.device, frag_shader, nullptr );
	vkDestroyShaderModule( gfx.device, vert_shader, nullptr );
	gpu_scene_data = gfx.allocate( sizeof( GpuSceneData ), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, "Scene Data Mesh Pipeline" );

	return {};
}

void WireframePipeline::cleanup( GfxDevice& gfx ) {
	vkDestroyPipelineLayout( gfx.device, layout, nullptr );
	vkDestroyDescriptorSetLayout( gfx.device, material_layout, nullptr );
	vkDestroyDescriptorSetLayout( gfx.device, scene_data_layout, nullptr );
	vkDestroyPipeline( gfx.device, pipeline, nullptr );
	gfx.free( gpu_scene_data );
}

DrawStats WireframePipeline::draw( GfxDevice& gfx, VkCommandBuffer cmd, const std::vector<MeshDrawCommand>& draw_commands, const GpuSceneData& scene_data ) const {
	DrawStats stats = {};

	GpuSceneData* gpu_scene_addr = nullptr;
	vmaMapMemory( gfx.allocator, gpu_scene_data.allocation, (void**)&gpu_scene_addr );
	*gpu_scene_addr = scene_data;
	vmaUnmapMemory( gfx.allocator, gpu_scene_data.allocation );

	VkDescriptorSet scene_data_descriptor_set = gfx.swapchain.getCurrentFrame( ).frame_descriptors.allocate( gfx.device, scene_data_layout );

	DescriptorWriter writer;
	writer.write_buffer( 0, gpu_scene_data.buffer, sizeof( GpuSceneData ), 0,
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER );
	writer.update_set( gfx.device, scene_data_descriptor_set );

	vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline );

	auto bindless_set = gfx.getBindlessSet( );

	vkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &bindless_set, 0, nullptr );
	vkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 1, 1, &scene_data_descriptor_set, 0, nullptr );

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

	for ( const auto& draw_command : draw_commands ) {

		vkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 2, 1,
			&draw_command.material->materialSet, 0, nullptr );

		vkCmdBindIndexBuffer( cmd, draw_command.index_buffer, 0, VK_INDEX_TYPE_UINT32 );

		PushConstants push_constants = {
			.world_from_local = draw_command.transform,
			.vertex_buffer_address = draw_command.vertex_buffer_address
		};
		vkCmdPushConstants( cmd, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof( PushConstants ), &push_constants );

		vkCmdDrawIndexed( cmd, draw_command.index_count, 1, draw_command.first_index, 0, 0 );

		stats.drawcall_count++;
		stats.triangle_count += draw_command.index_count / 3;
	}

	return stats;
}
