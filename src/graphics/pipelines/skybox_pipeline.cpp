#include "skybox_pipeline.h"

#include <vk_pipelines.h>
#include <vk_initializers.h>

using namespace vkinit;
using namespace vkutil;

SkyboxPipeline::Result<> SkyboxPipeline::init( GfxDevice& gfx ) {
	VkShaderModule frag_shader;
	if ( !load_shader_module( "../../shaders/skybox.frag.spv", gfx.device, &frag_shader ) ) {
		return std::unexpected( PipelineError{
			.error = Error::ShaderLoadingFailed,
			.message = "Failed to load skybox fragment shader!"
			} );
	}

	VkShaderModule vert_shader;
	if ( !load_shader_module( "../../shaders/skybox.vert.spv", gfx.device, &vert_shader ) ) {
		return std::unexpected( PipelineError{
			.error = Error::ShaderLoadingFailed,
			.message = "Failed to load skybox vertex shader!"
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
	builder.set_cull_mode( VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE );
	builder.set_multisampling_none( );
	builder.disable_blending( );	
	builder.enable_depthtest(false, VK_COMPARE_OP_LESS_OR_EQUAL );

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
		.pObjectName = "Skybox Pipeline"
	};
	vkSetDebugUtilsObjectNameEXT( gfx.device, &obj );
#endif

	vkDestroyShaderModule( gfx.device, frag_shader, nullptr );
	vkDestroyShaderModule( gfx.device, vert_shader, nullptr );

	gpu_scene_data = gfx.allocate( sizeof( GpuSceneData ), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, "Scene Data GBuffer Pipeline" );

	// create cube mesh
	createCubeMesh( gfx );

	return {};
}

void SkyboxPipeline::createCubeMesh( GfxDevice& gfx ) {
	std::vector<Mesh::Vertex> vertices = {
		// Front face
		{{-1.0f, 1.0f, 1.0f}, 0.0f, {0.0f, 0.0f, 1.0f}, 0.0f, {1.0f, 0.0f, 0.0f, 1.0f}},
		{{1.0f, 1.0f, 1.0f}, 1.0f, {0.0f, 0.0f, 1.0f}, 0.0f, {1.0f, 0.0f, 0.0f, 1.0f}},
		{{1.0f, -1.0f, 1.0f}, 1.0f, {0.0f, 0.0f, 1.0f}, 1.0f, {1.0f, 0.0f, 0.0f, 1.0f}},
		{{-1.0f, -1.0f, 1.0f}, 0.0f, {0.0f, 0.0f, 1.0f}, 1.0f, {1.0f, 0.0f, 0.0f, 1.0f}},

		// Back face
		{{-1.0f, 1.0f, -1.0f}, 1.0f, {0.0f, 0.0f, -1.0f}, 0.0f, {-1.0f, 0.0f, 0.0f, 1.0f}},
		{{1.0f, 1.0f, -1.0f}, 0.0f, {0.0f, 0.0f, -1.0f}, 0.0f, {-1.0f, 0.0f, 0.0f, 1.0f}},
		{{1.0f, -1.0f, -1.0f}, 0.0f, {0.0f, 0.0f, -1.0f}, 1.0f, {-1.0f, 0.0f, 0.0f, 1.0f}},
		{{-1.0f, -1.0f, -1.0f}, 1.0f, {0.0f, 0.0f, -1.0f}, 1.0f, {-1.0f, 0.0f, 0.0f, 1.0f}}
	};

	std::vector<uint32_t> indices = {
		// Front face
		0, 1, 2,
		2, 3, 0,

		// Right face
		1, 5, 6,
		6, 2, 1,

		// Back face
		5, 4, 7,
		7, 6, 5,

		// Left face
		4, 0, 3,
		3, 7, 4,

		// Top face
		4, 5, 1,
		1, 0, 4,

		// Bottom face
		3, 2, 6,
		6, 7, 3
	};

	Mesh mesh = {
		.vertices = vertices,
		.indices = indices
	};

	cube_mesh = gfx.mesh_codex.addMesh( gfx, mesh );
}

void SkyboxPipeline::cleanup( GfxDevice& gfx ) {
	vkDestroyPipelineLayout( gfx.device, layout, nullptr );
	vkDestroyPipeline( gfx.device, pipeline, NULL );
	gfx.free( gpu_scene_data );
}

void SkyboxPipeline::draw( GfxDevice& gfx, VkCommandBuffer cmd, ImageID skybox_texture, const GpuSceneData& scene_data ) const {
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

	auto& mesh = gfx.mesh_codex.getMesh( cube_mesh );
	vkCmdBindIndexBuffer( cmd, mesh.index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32 );

	VkBufferDeviceAddressInfo address_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
		.pNext = nullptr,
		.buffer = gpu_scene_data.buffer
	};
	auto gpu_scene_address = vkGetBufferDeviceAddress( gfx.device, &address_info );

	PushConstants push_constants = {
		.scene_data_address =  gpu_scene_address,
		.vertex_buffer_address = mesh.vertex_buffer_address,
		.texture_id = skybox_texture,
	};
	vkCmdPushConstants( cmd, layout, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof( PushConstants ), &push_constants );

	vkCmdDrawIndexed( cmd, mesh.index_count, 1, 0, 0, 0 );
}
