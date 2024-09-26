#include "imgui_pipeline.h"

#include <vk_pipelines.h>
#include <vk_initializers.h>
#include <algorithm>

using namespace vkinit;
using namespace vkutil;

static const int MAX_IDX_COUNT = 1000000;
static const int MAX_VTX_COUNT = 1000000;

ImGuiPipeline::Result<> ImGuiPipeline::init( GfxDevice& gfx ) {

	// ----------
	// Setup imgui resources in our engine
	auto& io = ImGui::GetIO( );
	io.BackendRendererName = "Vulkan Bindless Backend";
	io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

	// font
	{
		uint8_t* data = nullptr;
		int width = 0;
		int height = 0;
		io.Fonts->GetTexDataAsRGBA32( &data, &width, &height );
		font_texture_id = gfx.image_codex.loadImageFromData( "ImGui Font", data,
			VkExtent3D{ .width = (uint32_t)width, .height = (uint32_t)height, .depth = 1 },
			VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, false );
		io.Fonts->SetTexID( (ImTextureID)font_texture_id );
	}

	// buffers
	{
		for ( size_t i = 0; i < gfx.swapchain.FRAME_OVERLAP; i++ ) {
			GpuBuffer index_buffer = gfx.allocate( sizeof( ImDrawIdx ) * MAX_IDX_COUNT, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, "imgui index buffer" );
			index_buffers.push_back( index_buffer );
		}

		for ( size_t i = 0; i < gfx.swapchain.FRAME_OVERLAP; i++ ) {
			GpuBuffer vertex_buffer = gfx.allocate( sizeof( ImDrawVert ) * MAX_VTX_COUNT, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, "imgui vertex buffer" );
			vertex_buffers.push_back( vertex_buffer );
		}
	}


	VkShaderModule frag_shader;
	if ( !load_shader_module( "../../shaders/imgui.frag.spv", gfx.device, &frag_shader ) ) {
		return std::unexpected( PipelineError{
			.error = Error::ShaderLoadingFailed,
			.message = "Failed to load imgui fragment shader!"
			} );
	}

	VkShaderModule vert_shader;
	if ( !load_shader_module( "../../shaders/imgui.vert.spv", gfx.device, &vert_shader ) ) {
		return std::unexpected( PipelineError{
			.error = Error::ShaderLoadingFailed,
			.message = "Failed to load imgui vertex shader!"
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
	builder.set_cull_mode( VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE );
	builder.set_multisampling_none( );
	builder.enable_blending( VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA );
	builder.disable_depthtest( );

	builder.set_color_attachment_format( gfx.swapchain.format );
	builder._pipelineLayout = layout;
	pipeline = builder.build_pipeline( gfx.device );

#ifdef ENABLE_DEBUG_UTILS
	const VkDebugUtilsObjectNameInfoEXT obj = {
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
		.pNext = nullptr,
		.objectType = VkObjectType::VK_OBJECT_TYPE_PIPELINE,
		.objectHandle = (uint64_t)pipeline,
		.pObjectName = "ImGui Pipeline"
	};
	vkSetDebugUtilsObjectNameEXT( gfx.device, &obj );
#endif

	vkDestroyShaderModule( gfx.device, frag_shader, nullptr );
	vkDestroyShaderModule( gfx.device, vert_shader, nullptr );
	return {};
}

void ImGuiPipeline::cleanup( GfxDevice& gfx ) {
	vkDestroyPipelineLayout( gfx.device, layout, nullptr );
	vkDestroyPipeline( gfx.device, pipeline, nullptr );

	for ( const auto& buffer : index_buffers ) {
		gfx.free( buffer );
	}

	for ( const auto& buffer : vertex_buffers ) {
		gfx.free( buffer );
	}
}

void ImGuiPipeline::draw( GfxDevice& gfx, VkCommandBuffer cmd, ImDrawData* draw_data ) {

	assert( draw_data );
	if ( draw_data->TotalVtxCount == 0 ) {
		return;
	}

	// ----------
	// copy buffers
	const auto current_frame_index = gfx.swapchain.frame_number % gfx.swapchain.FRAME_OVERLAP;
	size_t index_offset = 0;
	size_t vertex_offset = 0;
	for ( size_t i = 0; i < draw_data->CmdListsCount; i++ ) {
		const auto& cmd_list = *draw_data->CmdLists[i];

		// index
		{
			uint8_t* index_buffer = nullptr;
			vmaMapMemory( gfx.allocator, index_buffers.at( current_frame_index ).allocation, (void**)&index_buffer );
			memcpy( index_buffer + sizeof( ImDrawIdx ) * index_offset, cmd_list.IdxBuffer.Data, sizeof( ImDrawIdx ) * cmd_list.IdxBuffer.Size );
			vmaUnmapMemory( gfx.allocator, index_buffers.at( current_frame_index ).allocation );
		}

		// vertex
		{
			uint8_t* vertex_buffer = nullptr;
			vmaMapMemory( gfx.allocator, vertex_buffers.at( current_frame_index ).allocation, (void**)&vertex_buffer );
			memcpy( vertex_buffer + sizeof( ImDrawVert ) * vertex_offset, cmd_list.VtxBuffer.Data, sizeof( ImDrawVert ) * cmd_list.VtxBuffer.Size );
			vmaUnmapMemory( gfx.allocator, vertex_buffers.at( current_frame_index ).allocation );
		}

		index_offset += cmd_list.IdxBuffer.Size;
		vertex_offset += cmd_list.VtxBuffer.Size;
	}



	vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline );

	auto bindless_set = gfx.getBindlessSet( );
	vkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &bindless_set, 0, nullptr );

	auto& target_image = gfx.image_codex.getImage( gfx.swapchain.getCurrentFrame( ).hdr_color );
	VkViewport viewport = {
		.x = 0,
		.y = 0,
		.width = static_cast<float>(target_image.extent.width),
		.height = static_cast<float>(target_image.extent.height),
		.minDepth = 0.0f,
		.maxDepth = 1.0f,
	};
	vkCmdSetViewport( cmd, 0, 1, &viewport );

	auto clip_offset = draw_data->DisplayPos;
	auto clip_scale = draw_data->FramebufferScale;

	size_t global_idx_offset = 0;
	size_t global_vtx_offset = 0;

	vkCmdBindIndexBuffer( cmd, index_buffers.at( current_frame_index ).buffer, 0, sizeof( ImDrawIdx ) == sizeof( std::uint16_t ) ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32 );

	for ( int cmd_list_id = 0; cmd_list_id < draw_data->CmdListsCount; cmd_list_id++ ) {
		const auto& cmd_list = *draw_data->CmdLists[cmd_list_id];
		for ( int cmd_id = 0; cmd_id < cmd_list.CmdBuffer.Size; cmd_id++ ) {
			const auto& im_cmd = cmd_list.CmdBuffer[cmd_id];
			if ( im_cmd.UserCallback && im_cmd.UserCallback != ImDrawCallback_ResetRenderState ) {
				im_cmd.UserCallback( &cmd_list, &im_cmd );
				continue;
			}

			if ( im_cmd.ElemCount == 0 ) {
				continue;
			}

			auto clip_min = ImVec2( (im_cmd.ClipRect.x - clip_offset.x) * clip_scale.x, (im_cmd.ClipRect.y - clip_offset.y) * clip_scale.y );
			auto clip_max = ImVec2( (im_cmd.ClipRect.z - clip_offset.x) * clip_scale.x, (im_cmd.ClipRect.w - clip_offset.y) * clip_scale.y );
			clip_min.x = std::clamp( clip_min.x, 0.0f, viewport.width );
			clip_max.x = std::clamp( clip_max.x, 0.0f, viewport.width );
			clip_min.y = std::clamp( clip_min.y, 0.0f, viewport.height );
			clip_max.y = std::clamp( clip_max.y, 0.0f, viewport.height );
			if ( clip_max.x <= clip_min.x || clip_max.y <= clip_min.y ) {
				continue;
			}

			auto texture_id = gfx.image_codex.getWhiteImageId( );
			if ( im_cmd.TextureId != ImTextureID( ) ) {
				texture_id = (ImageID)(im_cmd.TextureId);
			}

			bool is_srgb = true;
			const auto& texture = gfx.image_codex.getImage( texture_id );
			if ( texture.format == VK_FORMAT_R8G8B8A8_SRGB || texture.format == VK_FORMAT_R16G16B16A16_SFLOAT ) {
				is_srgb = false;
			}

			const auto scale = glm::vec2( 2.0f / draw_data->DisplaySize.x, 2.0f / draw_data->DisplaySize.y );
			const auto translate = glm::vec2( -1.0f - draw_data->DisplayPos.x * scale.x, -1.0f - draw_data->DisplayPos.y * scale.y );

			// set scissor
			const auto scissor_x = static_cast<std::int32_t>(clip_min.x);
			const auto scissor_y = static_cast<std::int32_t>(clip_min.y);
			const auto s_width = static_cast<std::uint32_t>(clip_max.x - clip_min.x);
			const auto s_height = static_cast<std::uint32_t>(clip_max.y - clip_min.y);
			const auto scissor = VkRect2D{
				.offset = {scissor_x, scissor_y},
				.extent = {s_width, s_height},
			};
			vkCmdSetScissor( cmd, 0, 1, &scissor );

			VkBufferDeviceAddressInfo address_info = {
				.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
				.pNext = nullptr,
				.buffer = vertex_buffers.at( current_frame_index ).buffer
			};
			auto gpu_vertex_address = vkGetBufferDeviceAddress( gfx.device, &address_info );

			PushConstants pc = {
				.vertex_buffer = gpu_vertex_address,
				.texture_id = (uint32_t)texture_id,
				.is_srgb = is_srgb,
				.offset = translate,
				.scale = scale
			};

			vkCmdPushConstants( cmd, layout, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof( PushConstants ), &pc );
			vkCmdDrawIndexed( cmd, im_cmd.ElemCount, 1, im_cmd.IdxOffset + global_idx_offset, im_cmd.VtxOffset + im_cmd.VtxOffset + global_vtx_offset, 0 );
		}

		global_idx_offset += cmd_list.IdxBuffer.Size;
		global_vtx_offset += cmd_list.VtxBuffer.Size;
	}
}
