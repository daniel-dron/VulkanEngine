#include "ibl_pipeline.h"

#include <vk_pipelines.h>
#include <vk_initializers.h>
#include <tracy/tracy/Tracy.hpp>

using namespace vkinit;
using namespace vkutil;

IblPipeline::Result<> IblPipeline::init( GfxDevice& gfx ) {
	VkShaderModule frag_shader;
	if ( !load_shader_module( "../../shaders/ibl.frag.spv", gfx.device, &frag_shader ) ) {
		return std::unexpected( PipelineError{
			.error = Error::ShaderLoadingFailed,
			.message = "Failed to load mesh fragment shader!"
			} );
	}

	VkShaderModule vert_shader;
	if ( !load_shader_module( "../../shaders/ibl.vert.spv", gfx.device, &vert_shader ) ) {
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
	builder.set_cull_mode( VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE );
	builder.set_multisampling_none( );
	builder.disable_blending( );
	builder.disable_depthtest( );
	builder.set_color_attachment_format( VK_FORMAT_R32G32B32A32_SFLOAT );
	builder.set_multiview( 6 );
	builder._pipelineLayout = layout;
	pipeline = builder.build_pipeline( gfx.device );

#ifdef ENABLE_DEBUG_UTILS
	const VkDebugUtilsObjectNameInfoEXT obj = {
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
		.pNext = nullptr,
		.objectType = VkObjectType::VK_OBJECT_TYPE_PIPELINE,
		.objectHandle = (uint64_t)pipeline,
		.pObjectName = "IBL Pipeline"
	};
	vkSetDebugUtilsObjectNameEXT( gfx.device, &obj );
#endif

	vkDestroyShaderModule( gfx.device, frag_shader, nullptr );
	vkDestroyShaderModule( gfx.device, vert_shader, nullptr );

	initCubeMesh( gfx );
	initPushConstants( gfx );

	return {};
}

void IblPipeline::cleanup( GfxDevice& gfx ) {
	vkDestroyPipelineLayout( gfx.device, layout, nullptr );
	vkDestroyPipeline( gfx.device, pipeline, NULL );
	gfx.free( gpu_matrices );
}

DrawStats IblPipeline::draw( GfxDevice& gfx, VkCommandBuffer cmd, ImageID equirectangular, ImageID dst_cubemap ) const {
	ZoneScopedN( "IBL Pass" );
	START_LABEL( cmd, "IBL Pass", vec4( 1.0f, 0.0f, 1.0f, 1.0f ) );

	using namespace vkinit;

	auto& cubemap = gfx.image_codex.getImage( dst_cubemap );
	VkClearValue clear_value = { 0.0f, 0.0f ,0.0f, 1.0f };
	VkRenderingAttachmentInfo color_attachment = attachment_info( cubemap.view, &clear_value );

	VkRenderingInfo render_info = {
		.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
		.pNext = nullptr,
		.renderArea = VkRect2D{ VkOffset2D{0, 0}, VkExtent2D{cubemap.extent.width, cubemap.extent.height}},
		.layerCount = 1,
		.viewMask = 0b111111,
		.colorAttachmentCount = 1,
		.pColorAttachments = &color_attachment,
		.pDepthAttachment = nullptr,
		.pStencilAttachment = nullptr,
	};
	vkCmdBeginRendering( cmd, &render_info );

	{
		vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline );

		auto bindless_set = gfx.getBindlessSet( );
		vkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &bindless_set, 0, nullptr );

		VkViewport viewport = {
			.x = 0,
			.y = 0,
			.width = static_cast<float>(cubemap.extent.width),
			.height = static_cast<float>(cubemap.extent.height),
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
				.width = cubemap.extent.width,
				.height = cubemap.extent.height
			}
		};
		vkCmdSetScissor( cmd, 0, 1, &scissor );

		auto& mesh = gfx.mesh_codex.getMesh( cube_mesh );
		vkCmdBindIndexBuffer( cmd, mesh.index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32 );

		vkCmdPushConstants( cmd, layout, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof( PushConstants ), &push_constants );
		vkCmdDrawIndexed( cmd, mesh.index_count, 1, 0, 0, 0 );
	}

	vkCmdEndRendering( cmd );

	END_LABEL( cmd );

	return {};
}

void IblPipeline::initCubeMesh( GfxDevice& gfx ) {
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

void IblPipeline::initPushConstants( GfxDevice& gfx ) {

	auto& gpu_mesh = gfx.mesh_codex.getMesh( cube_mesh );


	mat4 projection = glm::perspective( glm::radians( 90.0f ), 1.0f, 0.1f, 10.0f );
	Matrices matrices = {
		.projection = projection,
		.views = {
			glm::lookAt( glm::vec3( 0.0f, 0.0f, 0.0f ), glm::vec3( 1.0f,  0.0f,  0.0f ), glm::vec3( 0.0f, -1.0f,  0.0f ) ),
			glm::lookAt( glm::vec3( 0.0f, 0.0f, 0.0f ), glm::vec3( -1.0f,  0.0f,  0.0f ), glm::vec3( 0.0f, -1.0f,  0.0f ) ),
			glm::lookAt( glm::vec3( 0.0f, 0.0f, 0.0f ), glm::vec3( 0.0f,  1.0f,  0.0f ), glm::vec3( 0.0f,  0.0f,  1.0f ) ),
			glm::lookAt( glm::vec3( 0.0f, 0.0f, 0.0f ), glm::vec3( 0.0f, -1.0f,  0.0f ), glm::vec3( 0.0f,  0.0f, -1.0f ) ),
			glm::lookAt( glm::vec3( 0.0f, 0.0f, 0.0f ), glm::vec3( 0.0f,  0.0f,  1.0f ), glm::vec3( 0.0f, -1.0f,  0.0f ) ),
			glm::lookAt( glm::vec3( 0.0f, 0.0f, 0.0f ), glm::vec3( 0.0f,  0.0f, -1.0f ), glm::vec3( 0.0f, -1.0f,  0.0f ) )
		}
	};

	gpu_matrices = gfx.allocate( sizeof( Matrices ), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, "Matrices IBL data" );
	Matrices* gpu_matrices_addr = nullptr;
	vmaMapMemory( gfx.allocator, gpu_matrices.allocation, (void**)&gpu_matrices_addr );
	memcpy( gpu_matrices_addr, &matrices, sizeof( Matrices ) );
	vmaUnmapMemory( gfx.allocator, gpu_matrices.allocation );

	VkBufferDeviceAddressInfo address_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
		.pNext = nullptr,
		.buffer = gpu_matrices.buffer
	};
	auto gpu_matrices_address = vkGetBufferDeviceAddress( gfx.device, &address_info );

	push_constants = {
		.vertex_buffer_address = gpu_mesh.vertex_buffer_address,
		.matrices = gpu_matrices_address
	};
}

