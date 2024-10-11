#include "vk_engine.h"

#include <SDL.h>
#include <SDL_vulkan.h>
#include <VkBootstrap.h>
#include <vk_initializers.h>
#include <vk_pipelines.h>
#include <vk_types.h>
#include <vulkan/vulkan_core.h>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stack>
#include <thread>

#include "SDL_events.h"
#include "SDL_stdinc.h"
#include "SDL_video.h"
#include "fmt/core.h"
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/gtx/orthonormalize.hpp"
#include "glm/gtx/integer.hpp"
#include "glm/gtx/matrix_decompose.hpp"
#include "glm/packing.hpp"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"
#include <graphics/descriptors.h>

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#define TRACY_ENABLE
#include <glm/gtc/type_ptr.hpp>

#include "engine/input.h"
#include "imguizmo/ImGuizmo.h"
#include "tracy/TracyClient.cpp"
#include "tracy/tracy/Tracy.hpp"
#include <graphics/pipelines/compute_pipeline.h>

#include <engine/loader.h>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <random>

VulkanEngine* loaded_engine = nullptr;

// TODO: move
void GpuBuffer::Upload( GfxDevice& gfx, void* data, size_t size ) {
	void* mapped_buffer = {};

	vmaMapMemory( gfx.allocator, allocation, &mapped_buffer );
	memcpy( mapped_buffer, data, size );
	vmaUnmapMemory( gfx.allocator, allocation );
}

VkDeviceAddress GpuBuffer::GetDeviceAddress( GfxDevice& gfx ) {
	if ( device_address == 0 ) {
		VkBufferDeviceAddressInfo address_info = {
			.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
			.pNext = nullptr,
			.buffer = buffer
		};

		device_address = vkGetBufferDeviceAddress( gfx.device, &address_info );
	}

	return device_address;
}

VulkanEngine& VulkanEngine::get( ) { return *loaded_engine; }

void VulkanEngine::init( ) {
	// only one engine initialization is allowed with the application.
	assert( loaded_engine == nullptr );
	loaded_engine = this;

	initSdl( );

	initVulkan( );

	initDefaultData( );

	initImgui( );

	imgui_pipeline.init( *gfx );

	EG_INPUT.init( );

	initScene( );

	// everything went fine
	is_initialized = true;
}

void VulkanEngine::initSdl( ) {
	SDL_Init( SDL_INIT_VIDEO );

	constexpr auto window_flags =
		static_cast<SDL_WindowFlags>(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

	window = SDL_CreateWindow( "Vulkan Engine", SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED, window_extent.width,
		window_extent.height, window_flags );
}

void VulkanEngine::initVulkan( ) {
	gfx = std::make_unique<GfxDevice>( );

	if ( renderer_options.vsync ) {
		gfx->swapchain.present_mode = VK_PRESENT_MODE_FIFO_KHR;
	} else {
		gfx->swapchain.present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
	}

	gfx->init( window );

	main_deletion_queue.flush( );
}

void VulkanEngine::initImgui( ) {
	// 1: create descriptor pool for IMGUI
	//  the size of the pool is very oversize, but it's copied from imgui demo
	//  itself.
	VkDescriptorPoolSize pool_sizes[] = {
		{VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
		{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
		{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
		{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
		{VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
		{VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
		{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
		{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
		{VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000} };

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	pool_info.maxSets = 1000;
	pool_info.poolSizeCount = (uint32_t)std::size( pool_sizes );
	pool_info.pPoolSizes = pool_sizes;

	VkDescriptorPool imguiPool;
	VK_CHECK( vkCreateDescriptorPool( gfx->device, &pool_info, nullptr, &imguiPool ) );

	// 2: initialize imgui library

	// this initializes the core structures of imgui
	ImGui::CreateContext( );

	// this initializes imgui for SDL
	ImGui_ImplSDL2_InitForVulkan( window );

	// this initializes imgui for Vulkan
	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance = gfx->instance;
	init_info.PhysicalDevice = gfx->chosen_gpu;
	init_info.Device = gfx->device;
	init_info.Queue = gfx->graphics_queue;
	init_info.DescriptorPool = imguiPool;
	init_info.MinImageCount = 3;
	init_info.ImageCount = 3;
	init_info.UseDynamicRendering = true;

	// dynamic rendering parameters for imgui to use
	init_info.PipelineRenderingCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
	init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
	init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats =
		&gfx->swapchain.format;

	init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

	ImGui_ImplVulkan_Init( &init_info );

	ImGui_ImplVulkan_CreateFontsTexture( );

	auto& io = ImGui::GetIO( );
	io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	ImGui::StyleColorsDark( );

	ImGuiStyle& style = ImGui::GetStyle( );
	ImVec4* colors = style.Colors;

	colors[ImGuiCol_Text] = ImVec4( 0.80f, 0.80f, 0.80f, 1.00f );
	colors[ImGuiCol_TextDisabled] = ImVec4( 0.50f, 0.50f, 0.50f, 1.00f );
	colors[ImGuiCol_WindowBg] = ImVec4( 0.10f, 0.10f, 0.10f, 1.00f );
	colors[ImGuiCol_ChildBg] = ImVec4( 0.10f, 0.10f, 0.10f, 1.00f );
	colors[ImGuiCol_PopupBg] = ImVec4( 0.10f, 0.10f, 0.10f, 1.00f );
	colors[ImGuiCol_Border] = ImVec4( 0.40f, 0.40f, 0.40f, 1.00f );
	colors[ImGuiCol_BorderShadow] = ImVec4( 0.00f, 0.00f, 0.00f, 0.00f );
	colors[ImGuiCol_FrameBg] = ImVec4( 0.20f, 0.20f, 0.20f, 1.00f );
	colors[ImGuiCol_FrameBgHovered] = ImVec4( 0.30f, 0.30f, 0.30f, 1.00f );
	colors[ImGuiCol_FrameBgActive] = ImVec4( 0.40f, 0.40f, 0.40f, 1.00f );
	colors[ImGuiCol_TitleBg] = ImVec4( 0.30f, 0.30f, 0.30f, 1.00f );
	colors[ImGuiCol_TitleBgActive] = ImVec4( 0.40f, 0.40f, 0.40f, 1.00f );
	colors[ImGuiCol_TitleBgCollapsed] = ImVec4( 0.00f, 0.00f, 0.00f, 0.51f );
	colors[ImGuiCol_MenuBarBg] = ImVec4( 0.14f, 0.14f, 0.14f, 1.00f );
	colors[ImGuiCol_ScrollbarBg] = ImVec4( 0.02f, 0.02f, 0.02f, 0.53f );
	colors[ImGuiCol_ScrollbarGrab] = ImVec4( 0.31f, 0.31f, 0.31f, 1.00f );
	colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4( 0.41f, 0.41f, 0.41f, 1.00f );
	colors[ImGuiCol_ScrollbarGrabActive] = ImVec4( 0.51f, 0.51f, 0.51f, 1.00f );
	colors[ImGuiCol_CheckMark] = ImVec4( 0.98f, 0.26f, 0.26f, 1.00f );
	colors[ImGuiCol_SliderGrab] = ImVec4( 0.88f, 0.24f, 0.24f, 1.00f );
	colors[ImGuiCol_SliderGrabActive] = ImVec4( 0.98f, 0.26f, 0.26f, 1.00f );
	colors[ImGuiCol_Button] = ImVec4( 0.30f, 0.30f, 0.30f, 1.00f );
	colors[ImGuiCol_ButtonHovered] = ImVec4( 0.40f, 0.40f, 0.40f, 1.00f );
	colors[ImGuiCol_ButtonActive] = ImVec4( 0.50f, 0.50f, 0.50f, 1.00f );
	colors[ImGuiCol_Header] = ImVec4( 0.30f, 0.30f, 0.30f, 1.00f );
	colors[ImGuiCol_HeaderHovered] = ImVec4( 0.40f, 0.40f, 0.40f, 1.00f );
	colors[ImGuiCol_HeaderActive] = ImVec4( 0.50f, 0.50f, 0.50f, 1.00f );
	colors[ImGuiCol_Separator] = ImVec4( 0.43f, 0.43f, 0.50f, 0.50f );
	colors[ImGuiCol_SeparatorHovered] = ImVec4( 0.75f, 0.10f, 0.10f, 0.78f );
	colors[ImGuiCol_SeparatorActive] = ImVec4( 0.75f, 0.10f, 0.10f, 1.00f );
	colors[ImGuiCol_ResizeGrip] = ImVec4( 0.98f, 0.26f, 0.26f, 0.20f );
	colors[ImGuiCol_ResizeGripHovered] = ImVec4( 0.98f, 0.26f, 0.26f, 0.67f );
	colors[ImGuiCol_ResizeGripActive] = ImVec4( 0.98f, 0.26f, 0.26f, 0.95f );
	colors[ImGuiCol_TabHovered] = ImVec4( 0.40f, 0.40f, 0.40f, 1.00f );
	colors[ImGuiCol_Tab] = ImVec4( 0.30f, 0.30f, 0.30f, 1.00f );
	colors[ImGuiCol_TabSelected] = ImVec4( 0.50f, 0.50f, 0.50f, 1.00f );
	colors[ImGuiCol_TabSelectedOverline] = ImVec4( 0.98f, 0.26f, 0.26f, 1.00f );
	colors[ImGuiCol_TabDimmed] = ImVec4( 0.07f, 0.10f, 0.15f, 0.97f );
	colors[ImGuiCol_TabDimmedSelected] = ImVec4( 0.42f, 0.14f, 0.14f, 1.00f );
	colors[ImGuiCol_TabDimmedSelectedOverline] = ImVec4( 0.50f, 0.50f, 0.50f, 1.00f );
	colors[ImGuiCol_DockingPreview] = ImVec4( 0.98f, 0.26f, 0.26f, 0.70f );
	colors[ImGuiCol_DockingEmptyBg] = ImVec4( 0.20f, 0.20f, 0.20f, 1.00f );
	colors[ImGuiCol_PlotLines] = ImVec4( 0.61f, 0.61f, 0.61f, 1.00f );
	colors[ImGuiCol_PlotLinesHovered] = ImVec4( 1.00f, 0.43f, 0.35f, 1.00f );
	colors[ImGuiCol_PlotHistogram] = ImVec4( 0.90f, 0.70f, 0.00f, 1.00f );
	colors[ImGuiCol_PlotHistogramHovered] = ImVec4( 1.00f, 0.60f, 0.00f, 1.00f );
	colors[ImGuiCol_TableHeaderBg] = ImVec4( 0.19f, 0.19f, 0.20f, 1.00f );
	colors[ImGuiCol_TableBorderStrong] = ImVec4( 0.31f, 0.31f, 0.35f, 1.00f );
	colors[ImGuiCol_TableBorderLight] = ImVec4( 0.23f, 0.23f, 0.25f, 1.00f );
	colors[ImGuiCol_TableRowBg] = ImVec4( 0.00f, 0.00f, 0.00f, 0.00f );
	colors[ImGuiCol_TableRowBgAlt] = ImVec4( 1.00f, 1.00f, 1.00f, 0.06f );
	colors[ImGuiCol_TextLink] = ImVec4( 0.98f, 0.26f, 0.26f, 1.00f );
	colors[ImGuiCol_TextSelectedBg] = ImVec4( 0.98f, 0.26f, 0.26f, 0.35f );
	colors[ImGuiCol_DragDropTarget] = ImVec4( 1.00f, 1.00f, 0.00f, 0.90f );
	colors[ImGuiCol_NavHighlight] = ImVec4( 0.98f, 0.26f, 0.26f, 1.00f );
	colors[ImGuiCol_NavWindowingHighlight] = ImVec4( 1.00f, 1.00f, 1.00f, 0.70f );
	colors[ImGuiCol_NavWindowingDimBg] = ImVec4( 0.80f, 0.80f, 0.80f, 0.20f );
	colors[ImGuiCol_ModalWindowDimBg] = ImVec4( 0.80f, 0.80f, 0.80f, 0.35f );


	style.WindowBorderSize = 1.0f;
	style.ChildBorderSize = 1.0f;
	style.PopupBorderSize = 1.0f;
	style.FrameBorderSize = 1.0f;
	style.WindowRounding = 0.0f;
	style.ChildRounding = 0.0f;
	style.FrameRounding = 0.0f;
	style.PopupRounding = 0.0f;
	style.ScrollbarRounding = 0.0f;
	style.GrabRounding = 0.0f;
	style.TabRounding = 0.0f;
	style.WindowTitleAlign = ImVec2( 0.0f, 0.5f );
	style.ItemSpacing = ImVec2( 8, 4 );
	style.FramePadding = ImVec2( 4, 2 );

	// add the destroy the imgui created structures
	main_deletion_queue.pushFunction( [&, imguiPool]( ) {
		ImGui_ImplVulkan_Shutdown( );
		vkDestroyDescriptorPool( gfx->device, imguiPool, nullptr );
	} );
}

float random_range( float min, float max ) {
	static std::random_device rd;
	static std::mt19937 gen( rd( ) );

	std::uniform_real_distribution<float> dis( min, max );

	return dis( gen );
}

float lerp( float a, float b, float f ) {
	return a + f * (b - a);
}

void VulkanEngine::ConstructSSAOPipeline( ) {
	ssao_buffer = gfx->allocate( sizeof( SSAOSettings ), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, "SSAO Settings" );
	ssao_kernel = gfx->allocate( sizeof( vec3 ) * 64, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, "SSAO Kernel" );

	std::vector<glm::vec3> kernels;
	for ( int i = 0; i < ssao_settings.kernelSize; i++ ) {
		glm::vec3 sample( random_range( 0.0, 1.0 ) * 2.0 - 1.0, random_range( 0.0, 1.0 ) * 2.0 - 1.0, random_range( 0.0, 1.0 ) );
		sample = glm::normalize( sample );
		sample *= random_range( 0.0, 1.0 );

		float scale = (float)i / (float)ssao_settings.kernelSize;
		float scaleMul = lerp( 0.1f, 1.0f, scale * scale );
		sample *= scaleMul;

		kernels.push_back( sample );
	}
	ssao_kernel.Upload( *gfx, kernels.data( ), kernels.size( ) * sizeof( vec3 ) );

	std::vector<glm::vec4> noiseData;
	for ( int i = 0; i < 16; i++ ) {
		glm::vec3 noise( random_range( 0.0, 1.0 ), random_range( 0.0, 1.0 ), 0.0f );
		noiseData.push_back( glm::vec4( noise, 1.0f ) );
	}
	ssao_settings.noise_texture = gfx->image_codex.loadImageFromData( "SSAO Noise", noiseData.data( ), VkExtent3D{ 4, 4, 1 }, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT, false );

	ssao_settings.depth_texture = gfx->swapchain.getCurrentFrame( ).depth;
	ssao_settings.normal_texture = gfx->swapchain.getCurrentFrame( ).gbuffer.normal;
	ssao_settings.scene = gpu_scene_data.GetDeviceAddress( *gfx );
	ssao_buffer.Upload( *gfx, &ssao_settings, sizeof( SSAOSettings ) );

	// Create pipeline
	auto& shader = gfx->shader_storage->Get( "ssao", T_COMPUTE );
	shader.RegisterReloadCallback( [&]( VkShaderModule module ) {
		VK_CHECK( vkWaitForFences( gfx->device, 1, &gfx->swapchain.getCurrentFrame( ).fence, true, 1000000000 ) );
		ssao_pipeline.cleanup( *gfx );

		ActuallyConstructSSAOPipeline( );
	} );

	ActuallyConstructSSAOPipeline( );
}

void VulkanEngine::ActuallyConstructSSAOPipeline( ) {
	auto& shader = gfx->shader_storage->Get( "ssao", T_COMPUTE );
	ssao_pipeline.addDescriptorSetLayout( 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER );
	ssao_pipeline.addDescriptorSetLayout( 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER );
	ssao_pipeline.addDescriptorSetLayout( 2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE );
	ssao_pipeline.build( *gfx, shader.handle, "ssao compute" );

	ssao_set = gfx->AllocateSet( ssao_pipeline.GetLayout( ) );

	auto& ssao_image = gfx->image_codex.getImage( gfx->swapchain.getCurrentFrame( ).ssao );
	DescriptorWriter writer;
	writer.WriteBuffer( 0, ssao_buffer.buffer, sizeof( SSAOSettings ), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER );
	writer.WriteBuffer( 1, ssao_kernel.buffer, sizeof( vec3 ) * 64, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER );
	writer.WriteImage( 2, ssao_image.GetBaseView( ), nullptr, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE );
	writer.UpdateSet( gfx->device, ssao_set );
}

void VulkanEngine::ConstructBlurPipeline( ) {
	auto& shader = gfx->shader_storage->Get( "blur", T_COMPUTE );
	shader.RegisterReloadCallback( [&]( VkShaderModule module ) {
		VK_CHECK( vkWaitForFences( gfx->device, 1, &gfx->swapchain.getCurrentFrame( ).fence, true, 1000000000 ) );
		blur_pipeline.cleanup( *gfx );

		ActuallyConstructBlurPipeline( );
	} );

	ActuallyConstructBlurPipeline( );
}

void VulkanEngine::ActuallyConstructBlurPipeline( ) {
	auto& shader = gfx->shader_storage->Get( "blur", T_COMPUTE );
	blur_pipeline.addDescriptorSetLayout( 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE );
	blur_pipeline.addPushConstantRange( sizeof( BlurSettings ) );
	blur_pipeline.build( *gfx, shader.handle, "blur pipeline" );

	blur_set = gfx->AllocateSet( blur_pipeline.GetLayout( ) );
}

void VulkanEngine::resizeSwapchain( uint32_t width, uint32_t height ) {
	vkDeviceWaitIdle( gfx->device );

	window_extent.width = width;
	window_extent.height = height;

	gfx->swapchain.recreate( width, height );
}

void VulkanEngine::cleanup( ) {
	if ( is_initialized ) {
		// wait for gpu work to finish
		vkDeviceWaitIdle( gfx->device );

		pbr_pipeline.cleanup( *gfx );
		wireframe_pipeline.cleanup( *gfx );
		gbuffer_pipeline.cleanup( *gfx );
		imgui_pipeline.cleanup( *gfx );
		skybox_pipeline.cleanup( *gfx );
		shadowmap_pipeline.cleanup( *gfx );
		blur_pipeline.cleanup( *gfx );

		post_process_pipeline.cleanup( *gfx );
		ssao_pipeline.cleanup( *gfx );
		gfx->free( ssao_buffer );
		gfx->free( ssao_kernel );

		ibl.clean( *gfx );

		main_deletion_queue.flush( );

		gfx->cleanup( );

		SDL_DestroyWindow( window );
	}

	// clear engine pointer
	loaded_engine = nullptr;
}

void VulkanEngine::draw( ) {
	ZoneScopedN( "draw" );

	uint32_t swapchainImageIndex;
	{
		ZoneScopedN( "vsync" );
		// wait for last frame rendering phase. 1 sec timeout
		VK_CHECK( vkWaitForFences( gfx->device, 1, &gfx->swapchain.getCurrentFrame( ).fence, true, 1000000000 ) );

		gfx->swapchain.getCurrentFrame( ).deletion_queue.flush( );

		VkResult e = (vkAcquireNextImageKHR( gfx->device, gfx->swapchain.swapchain, 1000000000, gfx->swapchain.getCurrentFrame( ).swapchain_semaphore, 0, &swapchainImageIndex ));
		if ( e != VK_SUCCESS ) {
			return;
		}

		// Reset after acquire the next image from the swapchain
		// Incase of error, this fence would never get passed to the queue, thus
		// never triggering leaving us with a timeout next time we wait for fence
		VK_CHECK( vkResetFences( gfx->device, 1, &gfx->swapchain.getCurrentFrame( ).fence ) );
	}

	auto& color = gfx->image_codex.getImage( gfx->swapchain.getCurrentFrame( ).hdr_color );

	auto& depth = gfx->image_codex.getImage( gfx->swapchain.getCurrentFrame( ).depth );

	draw_extent.height = static_cast<uint32_t>(
		std::min( gfx->swapchain.extent.height, color.GetExtent( ).height ) *
		render_scale);
	draw_extent.width = static_cast<uint32_t>(
		std::min( gfx->swapchain.extent.width, color.GetExtent( ).width ) * render_scale);

	//
	// commands
	//
	auto cmd = gfx->swapchain.getCurrentFrame( ).command_buffer;

	// we can safely reset the command buffer because we waited for the render
	// fence
	VK_CHECK( vkResetCommandBuffer( cmd, 0 ) );

	// begin recording. will only use this command buffer once
	auto cmdBeginInfo = vkinit::command_buffer_begin_info( VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT );
	VK_CHECK( vkBeginCommandBuffer( cmd, &cmdBeginInfo ) );

	depth.TransitionLayout( cmd, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, true );

	if ( gfx->swapchain.frame_number == 5 ) {
	}

	ShadowMapPass( cmd );
	gbufferPass( cmd );
	SSAOPass( cmd );
	pbrPass( cmd );
	skyboxPass( cmd );
	postProcessPass( cmd );

	Image::TransitionLayout( cmd, gfx->swapchain.images[swapchainImageIndex], VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL );
	drawImgui( cmd, gfx->swapchain.views[swapchainImageIndex] );

	Image::TransitionLayout( cmd, gfx->swapchain.images[swapchainImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR );
	VK_CHECK( vkEndCommandBuffer( cmd ) );

	//
	// send commands
	//

	// wait on _swapchainSemaphore. signaled when the swap chain is ready
	// wait on _renderSemaphore. signaled when rendering has finished
	auto cmdinfo = vkinit::command_buffer_submit_info( cmd );

	auto waitInfo = vkinit::semaphore_submit_info( VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, gfx->swapchain.getCurrentFrame( ).swapchain_semaphore );
	auto signalInfo = vkinit::semaphore_submit_info( VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, gfx->swapchain.getCurrentFrame( ).render_semaphore );

	auto submit = vkinit::submit_info( &cmdinfo, &signalInfo, &waitInfo );

	// submit command buffer and execute it
	// _renderFence will now block until the commands finish
	VK_CHECK( vkQueueSubmit2( gfx->graphics_queue, 1, &submit, gfx->swapchain.getCurrentFrame( ).fence ) );

	//
	// present
	//
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext = nullptr;
	presentInfo.pSwapchains = &gfx->swapchain.swapchain;
	presentInfo.swapchainCount = 1;

	// wait on _renderSemaphore, since we need the rendering to have finished
	// to display to the screen
	presentInfo.pWaitSemaphores = &gfx->swapchain.getCurrentFrame( ).render_semaphore;
	presentInfo.waitSemaphoreCount = 1;

	presentInfo.pImageIndices = &swapchainImageIndex;

	vkQueuePresentKHR( gfx->graphics_queue, &presentInfo );

	// increase frame number for next loop
	gfx->swapchain.frame_number++;
}

void VulkanEngine::gbufferPass( VkCommandBuffer cmd ) const {
	ZoneScopedN( "GBuffer Pass" );
	START_LABEL( cmd, "GBuffer Pass", vec4( 1.0f, 1.0f, 0.0f, 1.0 ) );

	using namespace vkinit;

	// ----------
	// Attachments
	{
		auto& gbuffer = gfx->swapchain.getCurrentFrame( ).gbuffer;
		auto& albedo = gfx->image_codex.getImage( gbuffer.albedo );
		auto& normal = gfx->image_codex.getImage( gbuffer.normal );
		auto& position = gfx->image_codex.getImage( gbuffer.position );
		auto& pbr = gfx->image_codex.getImage( gbuffer.pbr );
		auto& depth = gfx->image_codex.getImage( gfx->swapchain.getCurrentFrame( ).depth );

		VkClearValue clear_color = { 0.0f, 0.0f, 0.0f, 1.0f };
		std::array<VkRenderingAttachmentInfo, 4> color_attachments = {
			attachment_info( albedo.GetBaseView( ), &clear_color ),
			attachment_info( normal.GetBaseView( ), &clear_color ),
			attachment_info( position.GetBaseView( ), &clear_color ),
			attachment_info( pbr.GetBaseView( ), &clear_color ),
		};
		VkRenderingAttachmentInfo depth_attachment = depth_attachment_info( depth.GetBaseView( ), VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL );

		VkRenderingInfo render_info = {
			.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
			.pNext = nullptr,
			.renderArea = VkRect2D { VkOffset2D{ 0, 0 }, draw_extent },
			.layerCount = 1,
			.colorAttachmentCount = color_attachments.size( ),
			.pColorAttachments = color_attachments.data( ),
			.pDepthAttachment = &depth_attachment,
			.pStencilAttachment = nullptr
		};
		vkCmdBeginRendering( cmd, &render_info );
	}

	// ----------
	// Call pipeline
	gbuffer_pipeline.draw( *gfx, cmd, draw_commands, scene_data );

	vkCmdEndRendering( cmd );

	END_LABEL( cmd );
}

void VulkanEngine::SSAOPass( VkCommandBuffer cmd ) const {
	ZoneScopedN( "SSAO Pass" );
	START_LABEL( cmd, "SSAO Pass", vec4( 1.0f, 0.5f, 0.3f, 1.0f ) );

	auto bindless = gfx->getBindlessSet( );
	auto& output = gfx->image_codex.getImage( gfx->swapchain.getCurrentFrame( ).ssao );

	output.TransitionLayout( cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL );

	ssao_pipeline.bind( cmd );
	ssao_pipeline.bindDescriptorSet( cmd, bindless, 0 );
	ssao_pipeline.bindDescriptorSet( cmd, ssao_set, 1 );
	ssao_pipeline.dispatch( cmd, (output.GetExtent( ).width + 15) / 16, (output.GetExtent( ).height + 15) / 16, 6 );

	output.TransitionLayout( cmd, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );

	// ----------
	// Blur
	output.TransitionLayout( cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL );
	DescriptorWriter writer;
	writer.WriteImage( 0, output.GetBaseView( ), nullptr, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE );
	writer.UpdateSet( gfx->device, blur_set );

	blur_settings.source_tex = output.GetId( );
	blur_settings.size = 2;

	blur_pipeline.bind( cmd );
	blur_pipeline.bindDescriptorSet( cmd, bindless, 0 );
	blur_pipeline.bindDescriptorSet( cmd, blur_set, 1 );
	blur_pipeline.pushConstants( cmd, sizeof( BlurSettings ), &blur_settings );
	blur_pipeline.dispatch( cmd, (output.GetExtent( ).width + 15) / 16, (output.GetExtent( ).height + 15) / 16, 6 );
	output.TransitionLayout( cmd, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );

	END_LABEL( cmd );
}

void VulkanEngine::pbrPass( VkCommandBuffer cmd ) const {
	ZoneScopedN( "PBR Pass" );
	START_LABEL( cmd, "PBR Pass", vec4( 1.0f, 0.0f, 1.0f, 1.0f ) );

	using namespace vkinit;

	{
		auto& image = gfx->image_codex.getImage( gfx->swapchain.getCurrentFrame( ).hdr_color );

		VkClearValue clear_color = { 0.0f, 0.0f, 0.0f, 0.0f };
		VkRenderingAttachmentInfo color_attachment = attachment_info( image.GetBaseView( ), &clear_color );

		VkRenderingInfo render_info = {
			.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
			.pNext = nullptr,
			.renderArea = VkRect2D { VkOffset2D { 0, 0 }, draw_extent },
			.layerCount = 1,
			.colorAttachmentCount = 1,
			.pColorAttachments = &color_attachment,
			.pDepthAttachment = nullptr,
			.pStencilAttachment = nullptr
		};
		vkCmdBeginRendering( cmd, &render_info );
	}

	pbr_pipeline.draw( *gfx, cmd, scene_data, gpu_directional_lights, gpu_point_lights, gfx->swapchain.getCurrentFrame( ).gbuffer, ibl.getIrradiance( ), ibl.getRadiance( ), ibl.getBRDF( ) );

	vkCmdEndRendering( cmd );

	END_LABEL( cmd );
}

void VulkanEngine::skyboxPass( VkCommandBuffer cmd ) const {
	ZoneScopedN( "Skybox Pass" );
	START_LABEL( cmd, "Skybox Pass", vec4( 0.0f, 1.0f, 0.0f, 1.0f ) );

	using namespace vkinit;

	{
		auto& image = gfx->image_codex.getImage( gfx->swapchain.getCurrentFrame( ).hdr_color );
		auto& depth = gfx->image_codex.getImage( gfx->swapchain.getCurrentFrame( ).depth );

		VkRenderingAttachmentInfo color_attachment = attachment_info( image.GetBaseView( ), nullptr );

		VkRenderingAttachmentInfo depth_attachment = {
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.pNext = nullptr,
			.imageView = depth.GetBaseView( ),
			.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
			.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		};
		VkRenderingInfo render_info = {
			.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
			.pNext = nullptr,
			.renderArea = VkRect2D { VkOffset2D { 0, 0 }, draw_extent },
			.layerCount = 1,
			.colorAttachmentCount = 1,
			.pColorAttachments = &color_attachment,
			.pDepthAttachment = &depth_attachment,
			.pStencilAttachment = nullptr
		};
		vkCmdBeginRendering( cmd, &render_info );
	}

	if ( renderer_options.render_irradiance_instead_skybox ) {
		skybox_pipeline.draw( *gfx, cmd, ibl.getIrradiance( ), scene_data );
	} else {
		skybox_pipeline.draw( *gfx, cmd, ibl.getSkybox( ), scene_data );
	}

	vkCmdEndRendering( cmd );

	END_LABEL( cmd );
}

void VulkanEngine::postProcessPass( VkCommandBuffer cmd ) const {
	auto bindless = gfx->getBindlessSet( );
	auto& output = gfx->image_codex.getImage( gfx->swapchain.getCurrentFrame( ).post_process_image );

	pp_config.hdr = gfx->swapchain.getCurrentFrame( ).hdr_color;

	output.TransitionLayout( cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL );

	post_process_pipeline.bind( cmd );
	post_process_pipeline.bindDescriptorSet( cmd, bindless, 0 );
	post_process_pipeline.bindDescriptorSet( cmd, post_process_set, 1 );
	post_process_pipeline.pushConstants( cmd, sizeof( PostProcessConfig ), &pp_config );
	post_process_pipeline.dispatch( cmd, (output.GetExtent( ).width + 15) / 16, (output.GetExtent( ).height + 15) / 16, 6 );

	output.TransitionLayout( cmd, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
}

void VulkanEngine::ShadowMapPass( VkCommandBuffer cmd ) const {
	ZoneScopedN( "ShadowMap Pass" );
	START_LABEL( cmd, "ShadowMap Pass", vec4( 0.0f, 1.0f, 0.0f, 1.0f ) );

	using namespace vkinit;

	{
		auto& depth = gfx->image_codex.getImage( scene->directional_lights.at( 0 ).shadow_map );

		VkRenderingAttachmentInfo depth_attachment = depth_attachment_info( depth.GetBaseView( ), VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL );
		VkRenderingInfo render_info = {
			.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
			.pNext = nullptr,
			.renderArea = VkRect2D { VkOffset2D { 0, 0 }, VkExtent2D{ 2048, 2048 } },
			.layerCount = 1,
			.colorAttachmentCount = 0,
			.pColorAttachments = nullptr,
			.pDepthAttachment = &depth_attachment,
			.pStencilAttachment = nullptr
		};
		vkCmdBeginRendering( cmd, &render_info );
	}

	shadowmap_pipeline.draw( *gfx, cmd, draw_commands, scene_data.light_proj, scene_data.light_view, scene->directional_lights.at( 0 ).shadow_map );

	vkCmdEndRendering( cmd );

	END_LABEL( cmd );

}

void VulkanEngine::initDefaultData( ) {
	initImages( );

	gpu_scene_data = gfx->allocate( sizeof( GpuSceneData ), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, "drawGeometry" );

	main_deletion_queue.pushFunction( [=, this]( ) {
		gfx->free( gpu_scene_data );
	} );

	pbr_pipeline.init( *gfx );
	wireframe_pipeline.init( *gfx );
	gbuffer_pipeline.init( *gfx );
	skybox_pipeline.init( *gfx );
	shadowmap_pipeline.init( *gfx );

	// post process pipeline
	auto& post_process_shader = gfx->shader_storage->Get( "post_process", T_COMPUTE );
	post_process_shader.RegisterReloadCallback( [&]( VkShaderModule shader ) {
		VK_CHECK( vkWaitForFences( gfx->device, 1, &gfx->swapchain.getCurrentFrame( ).fence, true, 1000000000 ) );
		post_process_pipeline.cleanup( *gfx );

		post_process_pipeline.addDescriptorSetLayout( 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE );
		post_process_pipeline.addPushConstantRange( sizeof( PostProcessConfig ) );
		post_process_pipeline.build( *gfx, post_process_shader.handle, "post process compute" );
		post_process_set = gfx->AllocateSet( post_process_pipeline.GetLayout( ) );

		// TODO: this everyframe for more than one inflight frame
		DescriptorWriter writer;
		auto& out_image = gfx->image_codex.getImage( gfx->swapchain.getCurrentFrame( ).post_process_image );
		writer.WriteImage( 0, out_image.GetBaseView( ), nullptr, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE );
		writer.UpdateSet( gfx->device, post_process_set );
	} );

	post_process_pipeline.addDescriptorSetLayout( 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE );
	post_process_pipeline.addPushConstantRange( sizeof( PostProcessConfig ) );
	post_process_pipeline.build( *gfx, post_process_shader.handle, "post process compute" );
	post_process_set = gfx->AllocateSet( post_process_pipeline.GetLayout( ) );

	// TODO: this everyframe for more than one inflight frame
	DescriptorWriter writer;
	auto& out_image = gfx->image_codex.getImage( gfx->swapchain.getCurrentFrame( ).post_process_image );
	writer.WriteImage( 0, out_image.GetBaseView( ), nullptr, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE );
	writer.UpdateSet( gfx->device, post_process_set );

	// SSAO pipeline
	ConstructSSAOPipeline( );
	ConstructBlurPipeline( );
}

void VulkanEngine::initImages( ) {
	// 3 default textures, white, grey, black. 1 pixel each
	uint32_t white = glm::packUnorm4x8( glm::vec4( 1, 1, 1, 1 ) );
	white_image = gfx->image_codex.loadImageFromData( "debug_white_img", (void*)&white, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, false );

	uint32_t grey = glm::packUnorm4x8( glm::vec4( 0.66f, 0.66f, 0.66f, 1 ) );
	grey_image = gfx->image_codex.loadImageFromData( "debug_grey_img", (void*)&grey, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, false );

	uint32_t black = glm::packUnorm4x8( glm::vec4( 0, 0, 0, 0 ) );
	black_image = gfx->image_codex.loadImageFromData( "debug_black_img", (void*)&white, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, false );

	// checkerboard image
	uint32_t magenta = glm::packUnorm4x8( glm::vec4( 1, 0, 1, 1 ) );
	std::array<uint32_t, 16 * 16> pixels;  // for 16x16 checkerboard texture
	for ( int x = 0; x < 16; x++ ) {
		for ( int y = 0; y < 16; y++ ) {
			pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
		}
	}
	error_checkerboard_image = gfx->image_codex.loadImageFromData( "debug_checkboard_img", (void*)&white, VkExtent3D{ 16, 16, 1 }, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, false );

	VkSamplerCreateInfo sampl = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };

	sampl.magFilter = VK_FILTER_NEAREST;
	sampl.minFilter = VK_FILTER_NEAREST;

	vkCreateSampler( gfx->device, &sampl, nullptr, &default_sampler_nearest );

	sampl.magFilter = VK_FILTER_LINEAR;
	sampl.minFilter = VK_FILTER_LINEAR;
	vkCreateSampler( gfx->device, &sampl, nullptr, &default_sampler_linear );

	main_deletion_queue.pushFunction( [&]( ) {
		vkDestroySampler( gfx->device, default_sampler_nearest, nullptr );
		vkDestroySampler( gfx->device, default_sampler_linear, nullptr );
	} );
}

void VulkanEngine::initScene( ) {
	ibl.init( *gfx, "../../assets/texture/ibls/belfast_sunset_4k.hdr" );

	scene = GltfLoader::load( *gfx, "../../assets/sponza.glb" );

	// init camera
	if ( scene->cameras.empty( ) ) {
		camera = std::make_unique<Camera>( vec3{ 0.225f, 0.138f, -0.920 }, 6.5f, 32.0f, WIDTH, HEIGHT );
	} else {
		auto& c = scene->cameras[0];
		camera = std::make_unique<Camera>( c );
		camera->setAspectRatio( WIDTH, HEIGHT );
	}

	fps_controller = std::make_unique<FirstPersonFlyingController>( camera.get( ), 0.1f, 5.0f );
	camera_controller = fps_controller.get( );
}

// Global variables to store FPS history
std::vector<float> fpsHistory;
std::vector<float> frameTimeHistory;
const int historySize = 100;

void draw_fps_graph( bool useGraph = false ) {
	// Set window size and flags
	ImGui::SetNextWindowSize( ImVec2( 400, 250 ), ImGuiCond_Always );
	ImGui::SetNextWindowSizeConstraints( ImVec2( 400, 250 ), ImVec2( 400, 250 ) );
	ImGui::Begin( "FPS Display", nullptr,
		ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse );

	// Calculate FPS and frame time
	float fps = ImGui::GetIO( ).Framerate;
	float frameTime = 1000.0f / fps;

	// Update history
	fpsHistory.push_back( fps );
	frameTimeHistory.push_back( frameTime );
	if ( fpsHistory.size( ) > historySize ) {
		fpsHistory.erase( fpsHistory.begin( ) );
		frameTimeHistory.erase( frameTimeHistory.begin( ) );
	}

	if ( !useGraph ) {
		// Simple text version
		ImGui::Text( "FPS: %.1f", fps );
		ImGui::Text( "Frame Time: %.3f ms", frameTime );
	} else {
		// Graph version
		ImGui::PlotLines( "FPS", fpsHistory.data( ), (int)fpsHistory.size( ), 0,
			nullptr, 0.0f, 200.0f, ImVec2( 0, 80 ) );
		ImGui::PlotLines( "Frame Time (ms)", frameTimeHistory.data( ),
			(int)frameTimeHistory.size( ), 0, nullptr, 0.0f, 33.3f,
			ImVec2( 0, 80 ) );

		// Display current values
		ImGui::Text( "Current FPS: %.1f", fps );
		ImGui::Text( "Current Frame Time: %.3f ms", frameTime );
	}

	ImGui::End( );
}

std::shared_ptr<Scene::Node> selected_node = nullptr;
void drawNodeHierarchy( const std::shared_ptr<Scene::Node>& node ) {
	if ( !node ) return;  // Safety check in case of nullptr

	// Create a unique label for each tree node to avoid ID conflicts
	std::string label = node->name.empty( ) ? "Unnamed Node" : node->name;
	label += "##" + std::to_string( reinterpret_cast<uintptr_t>(node.get( )) );

	// Check if the node has children
	if ( !node->children.empty( ) ) {
		// Create a collapsible tree node for this scene node
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
		if ( selected_node == node ) {
			flags |= ImGuiTreeNodeFlags_Selected;
		}
		bool node_open = ImGui::TreeNodeEx( label.c_str( ), flags );

		// Check if this node is clicked
		if ( ImGui::IsItemClicked( ) ) {
			selected_node = node;
		}

		// Highlight the selected node
		if ( node == selected_node ) {
			ImGui::SetItemDefaultFocus( );
		}

		// Recursively draw children nodes if the node is open
		if ( node_open ) {
			for ( const auto& child : node->children ) {
				drawNodeHierarchy( child );
			}
			ImGui::TreePop( );
		}
	} else {
		// For nodes without children, create a selectable tree leaf
		if ( ImGui::Selectable( label.c_str( ), selected_node == node ) ) {
			selected_node = node;
			ImGui::SetItemDefaultFocus( );
		}

	}
}

static void drawSceneHierarchy( Scene::Node& node ) {
	node.transform.drawDebug( node.name );

	for ( auto& n : node.children ) {
		drawSceneHierarchy( *n.get( ) );
	}
};

void VulkanEngine::run( ) {
	bool bQuit = false;

	static ImageID selected_set = gfx->swapchain.getCurrentFrame( ).post_process_image;
	static int selected_set_n = 0;

	// main loop
	while ( !bQuit ) {
		FrameMarkNamed( "main" );
		// begin clock
		auto start = std::chrono::system_clock::now( );

		{
			ZoneScopedN( "poll_events" );
			EG_INPUT.poll_events( this );
		}

		if ( EG_INPUT.should_quit( ) ) {
			bQuit = true;
		}

		if ( EG_INPUT.was_key_pressed( EG_KEY::ESCAPE ) ) {
			bQuit = true;
		}

		static int savedMouseX, savedMouseY;
		// hide mouse
		if ( EG_INPUT.was_key_pressed( EG_KEY::MOUSE_RIGHT ) ) {
			SDL_GetMouseState( &savedMouseX, &savedMouseY );
			SDL_SetRelativeMouseMode( SDL_TRUE );
			SDL_ShowCursor( SDL_DISABLE );
		}
		// reveal mouse
		if ( EG_INPUT.was_key_released( EG_KEY::MOUSE_RIGHT ) ) {
			SDL_SetRelativeMouseMode( SDL_FALSE );
			SDL_ShowCursor( SDL_ENABLE );
			SDL_WarpMouseInWindow( window, savedMouseX, savedMouseY );
		}

		if ( EG_INPUT.was_key_pressed( EG_KEY::K ) ) {
			VK_CHECK( vkWaitForFences( gfx->device, 1, &gfx->swapchain.getCurrentFrame( ).fence, true, 1000000000 ) );
			pbr_pipeline.cleanup( *gfx );
			pbr_pipeline = PbrPipeline( );
			pbr_pipeline.init( *gfx );
		}

		if ( dirt_swapchain ) {
			gfx->swapchain.recreate( gfx->swapchain.extent.width, gfx->swapchain.extent.height );
			dirt_swapchain = false;
		}

		camera_controller->update( stats.frametime / 1000.0f );

		// do not draw if we are minimized
		if ( stop_rendering ) {
			// throttle the speed to avoid the endless spinning
			std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
			continue;
		}

		updateScene( );

		ImGui_ImplVulkan_NewFrame( );
		ImGui_ImplSDL2_NewFrame( );
		ImGuizmo::SetOrthographic( false );

		ImGui::NewFrame( );
		ImGuizmo::BeginFrame( );
		{
			// ----------
			// dockspace
			ImGui::DockSpaceOverViewport( 0, ImGui::GetMainViewport( ) );

			ImGui::ShowDemoWindow( );

			// Push a style to remove the window padding
			ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 0, 0 ) );
			if ( ImGui::Begin( "Viewport", 0, ImGuiWindowFlags_NoScrollbar ) ) {
				ImVec2 viewport_size = ImGui::GetContentRegionAvail( );

				float aspect_ratio = 16.0f / 9.0f;
				ImVec2 image_size;

				if ( viewport_size.x / viewport_size.y > aspect_ratio ) {
					image_size.y = viewport_size.y;
					image_size.x = image_size.y * aspect_ratio;
				} else {
					image_size.x = viewport_size.x;
					image_size.y = image_size.x / aspect_ratio;
				}

				ImVec2 image_pos(
					(viewport_size.x - image_size.x) * 0.5f,
					(viewport_size.y - image_size.y) * 0.5f
				);

				ImGui::SetCursorPos( image_pos );
				ImGui::Image( (ImTextureID)(selected_set), image_size );

				if ( selected_node != nullptr ) {
					ImGuizmo::SetOrthographic( false );
					ImGuizmo::SetDrawlist( );
					ImGuizmo::SetRect( ImGui::GetWindowPos( ).x, ImGui::GetWindowPos( ).y, (float)ImGui::GetWindowWidth( ), (float)ImGui::GetWindowHeight( ) );

					auto camera_view = scene_data.view;
					auto camera_proj = scene_data.proj;
					camera_proj[1][1] *= -1;

					auto tc = selected_node->getTransformMatrix( );
					ImGuizmo::Manipulate( glm::value_ptr( camera_view ), glm::value_ptr( camera_proj ), ImGuizmo::OPERATION::UNIVERSAL, ImGuizmo::MODE::WORLD, glm::value_ptr( tc ) );

					mat4 local_transform = tc;
					if ( auto parent = selected_node->parent.lock( ) ) {
						mat4 parent_world_inverse = glm::inverse( parent->getTransformMatrix( ) );
						local_transform = parent_world_inverse * tc;
					}

					selected_node->setTransform( local_transform );
				}
				//else {
				//	ImGuizmo::SetOrthographic( false );
				//	ImGuizmo::SetDrawlist( );
				//	ImGuizmo::SetRect( ImGui::GetWindowPos( ).x, ImGui::GetWindowPos( ).y, (float)ImGui::GetWindowWidth( ), (float)ImGui::GetWindowHeight( ) );

				//	auto camera_view = scene_data.view;
				//	auto camera_proj = scene_data.proj;
				//	camera_proj[1][1] *= -1;

				//	auto& light = directional_lights.at( 0 );
				//	auto model = light.transform.model;
				//	if ( ImGuizmo::Manipulate( glm::value_ptr( camera_view ), glm::value_ptr( camera_proj ), ImGuizmo::OPERATION::ROTATE, ImGuizmo::MODE::WORLD, glm::value_ptr( model ) ) ) {
				//		glm::mat3 rotationMat = glm::mat3( model );
				//		light.transform.euler = glm::eulerAngles( glm::quat_cast( rotationMat ) );
				//		light.transform.model = model;
				//	}
				//}
			}
			ImGui::End( );

			// Pop the style change
			ImGui::PopStyleVar( );

			if ( ImGui::Begin( "Scene" ) ) {
				drawNodeHierarchy( scene->top_nodes[0] );
			}
			ImGui::End( );

			if ( EG_INPUT.was_key_pressed( EG_KEY::Z ) ) {
				ImGui::OpenPopup( "Viewport Context" );
			}
			if ( ImGui::BeginPopup( "Viewport Context" ) ) {
				ImGui::SeparatorText( "GBuffer" );
				if ( ImGui::RadioButton( "PBR Pass", &selected_set_n, 0 ) ) {
					selected_set = gfx->swapchain.getCurrentFrame( ).post_process_image;
				}
				if ( ImGui::RadioButton( "Albedo", &selected_set_n, 1 ) ) {
					selected_set = gfx->swapchain.getCurrentFrame( ).gbuffer.albedo;
				}
				if ( ImGui::RadioButton( "Position", &selected_set_n, 2 ) ) {
					selected_set = gfx->swapchain.getCurrentFrame( ).gbuffer.position;
				}
				if ( ImGui::RadioButton( "Normal", &selected_set_n, 3 ) ) {
					selected_set = gfx->swapchain.getCurrentFrame( ).gbuffer.normal;
				}
				if ( ImGui::RadioButton( "PBR", &selected_set_n, 4 ) ) {
					selected_set = gfx->swapchain.getCurrentFrame( ).gbuffer.pbr;
				}
				if ( ImGui::RadioButton( "HDR", &selected_set_n, 5 ) ) {
					selected_set = gfx->swapchain.getCurrentFrame( ).hdr_color;
				}
				if ( ImGui::RadioButton( "ShadowMap", &selected_set_n, 6 ) ) {
					selected_set = scene->directional_lights.at( 0 ).shadow_map;
				}
				if ( ImGui::RadioButton( "Depth", &selected_set_n, 7 ) ) {
					selected_set = gfx->swapchain.getCurrentFrame( ).depth;
				}
				if ( ImGui::RadioButton( "SSAO", &selected_set_n, 8 ) ) {
					selected_set = gfx->swapchain.getCurrentFrame( ).ssao;
				}
				ImGui::Separator( );
				ImGui::SliderFloat( "Render Scale", &render_scale, 0.3f, 1.f );
				ImGui::DragFloat( "Exposure", &pp_config.exposure, 0.001f, 0.00f, 10.0f );
				ImGui::DragFloat( "Gamma", &pp_config.gamma, 0.01f, 0.01f, 10.0f );
				ImGui::Checkbox( "Wireframe", &renderer_options.wireframe );
				ImGui::Checkbox( "Frustum Culling", &renderer_options.frustum );
				ImGui::Checkbox( "Render Irradiance Map", &renderer_options.render_irradiance_instead_skybox );
				if ( ImGui::Checkbox( "VSync", &renderer_options.vsync ) ) {
					if ( renderer_options.vsync ) {
						gfx->swapchain.present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
					} else {
						gfx->swapchain.present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
					}
					dirt_swapchain = true;
				}
				ImGui::EndPopup( );
			}

			if ( ImGui::Begin( "Settings" ) ) {
				if ( ImGui::CollapsingHeader( "Node" ) ) {
					ImGui::Indent( );
					if ( selected_node ) {
						if ( ImGui::Button( "Deselect" ) ) {
							selected_node = nullptr;
						}

						if ( selected_node ) {
							selected_node->transform.drawDebug( selected_node->name );
						}

						for ( auto& light : scene->point_lights ) {
							if ( light.node == selected_node ) {
								light.DrawDebug( );
							}
						}
					}
					ImGui::Unindent( );
				}

				if ( ImGui::CollapsingHeader( "GPU Info" ) ) {
					ImGui::Indent( );
					gfx->DrawDebug( );
					ImGui::Unindent( );
				}

				if ( ImGui::CollapsingHeader( "Camera" ) ) {
					ImGui::SeparatorText( "Camera 3D" );
					camera->drawDebug( );

					ImGui::SeparatorText( "Camera Controller" );
					camera_controller->draw_debug( );
				}

				if ( ImGui::CollapsingHeader( "Renderer" ) ) {
					ImGui::Indent( );

					pbr_pipeline.DrawDebug( );

					ImGui::Indent( );
					// ssao settings
					ImGui::Checkbox( "SSAO", &ssao_settings.enable );
					ImGui::DragFloat( "SSAO Radius", &ssao_settings.radius, 0.01f, 0.0f, 1.0f );
					ImGui::DragFloat( "SSAO Bias", &ssao_settings.bias, 0.01f, 0.0f, 1.0f );
					ImGui::DragFloat( "SSAO Power", &ssao_settings.power, 0.01f, 0.0f, 1.0f );

					ImGui::Unindent( );

					ImGui::Unindent( );
				}

				if ( ImGui::CollapsingHeader( "Image Codex" ) ) {
					ImGui::Indent( );
					gfx->image_codex.DrawDebug( );
					ImGui::Unindent( );
				}

				if ( ImGui::CollapsingHeader( "Directional Lights" ) ) {
					ImGui::Indent( );
					for ( auto i = 0; i < scene->directional_lights.size( ); i++ ) {
						if ( ImGui::CollapsingHeader( std::format( "Sun {}", i ).c_str( ) ) ) {
							ImGui::PushID( i );

							auto& light = scene->directional_lights.at( i );
							ImGui::ColorEdit3( "Color HSV", &light.hsv.hue, ImGuiColorEditFlags_DisplayHSV | ImGuiColorEditFlags_InputHSV | ImGuiColorEditFlags_PickerHueWheel );
							ImGui::DragFloat( "Power", &light.power, 0.1f );

							auto euler = glm::degrees(light.node->transform.euler );
							if ( ImGui::DragFloat3( "Rotation", glm::value_ptr( euler ) ) ) {
								light.node->transform.euler = glm::radians( euler );
							}

							auto shadow_map_pos = glm::normalize( light.node->transform.asMatrix( ) * glm::vec4( 0, 0, -1, 0 ) ) * light.distance;
							ImGui::DragFloat3( "Pos", glm::value_ptr( shadow_map_pos ) );

							ImGui::DragFloat( "Distance", &light.distance );
							ImGui::DragFloat( "Right", &light.right );
							ImGui::DragFloat( "Up", &light.up );
							ImGui::DragFloat( "Near", &light.near_plane );
							ImGui::DragFloat( "Far", &light.far_plane );

							ImGui::Image( (ImTextureID)(light.shadow_map), ImVec2( 200.0f, 200.0f ) );

							ImGui::PopID( );
						}
					}
					ImGui::Unindent( );
				}
			}
			ImGui::End( );

			if ( ImGui::Begin( "Stats" ) ) {
				ImGui::Text( "frametime %f ms", stats.frametime );
				ImGui::Text( "draw time %f ms", stats.mesh_draw_time );
				ImGui::Text( "update time %f ms", stats.scene_update_time );
				ImGui::Text( "triangles %i", stats.triangle_count );
				ImGui::Text( "draws %i", stats.drawcall_count );
			}
			ImGui::End( );
		}

		ImGui::Render( );

		draw( );

		// get clock again, compare with start clock
		auto end = std::chrono::system_clock::now( );

		// convert to microseconds (integer), and then come back to miliseconds
		auto elapsed =
			std::chrono::duration_cast<std::chrono::microseconds>(end - start);
		stats.frametime = elapsed.count( ) / 1000.f;

		if ( timer >= 500.0f ) {
			gfx->shader_storage->Reconstruct( );
			timer = 0.0f;
		}

		timer += stats.frametime;
	}
}

void VulkanEngine::drawImgui( VkCommandBuffer cmd, VkImageView target_image_view ) {
	{
		using namespace vkinit;

		VkClearValue clear_color = { 0.0f, 0.0f, 0.0f, 1.0f };
		VkRenderingAttachmentInfo color_attachment = attachment_info( target_image_view, &clear_color );

		VkRenderingInfo render_info = {
			.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
			.pNext = nullptr,
			.renderArea = VkRect2D { VkOffset2D{ 0, 0 }, draw_extent },
			.layerCount = 1,
			.colorAttachmentCount = 1,
			.pColorAttachments = &color_attachment,
			.pDepthAttachment = nullptr,
			.pStencilAttachment = nullptr
		};
		vkCmdBeginRendering( cmd, &render_info );
	}

	imgui_pipeline.draw( *gfx, cmd, ImGui::GetDrawData( ) );

	vkCmdEndRendering( cmd );
}

static void createDrawCommands( GfxDevice& gfx, const Scene& scene, const Scene::Node& node, std::vector<MeshDrawCommand>& draw_commands ) {
	if ( !node.mesh_ids.empty( ) ) {
		for ( auto mesh_id : node.mesh_ids ) {
			auto& mesh_asset = scene.meshes[mesh_id];

			auto& mesh = gfx.mesh_codex.getMesh( mesh_asset.mesh );

			MeshDrawCommand mdc = {
				.index_buffer = mesh.index_buffer.buffer,
				.index_count = mesh.index_count,
				.vertex_buffer_address = mesh.vertex_buffer_address,
				.world_from_local = node.getTransformMatrix( ),
				.material_id = scene.materials[mesh_asset.material]
			};
			draw_commands.push_back( mdc );
		}
	}

	for ( auto& n : node.children ) {
		createDrawCommands( gfx, scene, *n.get( ), draw_commands );
	}
}

vec3 GetDirectionVector( const glm::vec3& rotation ) {
	glm::vec3 radiansAngles = glm::radians( rotation );
	glm::mat4 rotX = glm::rotate( glm::mat4( 1.0f ), radiansAngles.x, glm::vec3( 1, 0, 0 ) );
	glm::mat4 rotY = glm::rotate( glm::mat4( 1.0f ), radiansAngles.y, glm::vec3( 0, 1, 0 ) );
	glm::mat4 rotZ = glm::rotate( glm::mat4( 1.0f ), radiansAngles.z, glm::vec3( 0, 0, 1 ) );
	glm::mat4 rotationMatrix = rotZ * rotY * rotX;
	glm::vec4 direction = rotationMatrix * glm::vec4( 0, 0, -1, 0 );
	return glm::normalize( glm::vec3( direction ) );
}

void VulkanEngine::updateScene( ) {
	ZoneScopedN( "update_scene" );
	auto start = std::chrono::system_clock::now( );

	draw_commands.clear( );
	createDrawCommands( *gfx.get( ), *scene, *(scene->top_nodes[0].get( )), draw_commands );

	// camera
	scene_data.view = camera->getViewMatrix( );
	scene_data.proj = camera->getProjectionMatrix( );
	scene_data.viewproj = scene_data.proj * scene_data.view;
	scene_data.camera_position = vec4( camera->getPosition( ), 0.0f );
	scene_data.shadow_map = scene->directional_lights.at( 0 ).shadow_map;
	auto& light = scene->directional_lights.at( 0 );
	auto proj = glm::ortho( -light.right, light.right, -light.up, light.up, light.near_plane, light.far_plane );

	auto shadow_map_pos = glm::normalize( light.node->getTransformMatrix( ) * glm::vec4( 0, 0, 1, 0 ) ) * light.distance;
	auto view = glm::lookAt( vec3( shadow_map_pos ), vec3( 0.0f, 0.0f, 0.0f ), GlobalUp );
	scene_data.light_proj = proj;
	scene_data.light_view = view;

	gpu_directional_lights.clear( );
	scene_data.number_of_directional_lights = static_cast<int>(scene->directional_lights.size( ));
	for ( auto i = 0; i < scene->directional_lights.size( ); i++ ) {
		GpuDirectionalLight gpu_light;
		auto& light = scene->directional_lights.at( i );

		ImGui::ColorConvertHSVtoRGB( light.hsv.hue, light.hsv.saturation, light.hsv.value, gpu_light.color.x, gpu_light.color.y, gpu_light.color.z );
		gpu_light.color *= light.power;

		// get direction
		auto direction = light.node->getTransformMatrix( ) * glm::vec4( 0, 0, 1, 0 );
		gpu_light.direction = direction;
		gpu_directional_lights.push_back( gpu_light );
	}

	gpu_point_lights.clear( );
	scene_data.number_of_point_lights = static_cast<int>(scene->point_lights.size( ));
	for ( auto i = 0; i < scene->point_lights.size( ); i++ ) {
		GpuPointLightData gpu_light;
		auto& light = scene->point_lights.at( i );

		gpu_light.position = light.node->transform.position;

		ImGui::ColorConvertHSVtoRGB( light.hsv.hue, light.hsv.saturation, light.hsv.value, gpu_light.color.x, gpu_light.color.y, gpu_light.color.z );
		gpu_light.color *= light.power;

		gpu_light.quadratic = light.quadratic;
		gpu_light.linear = light.linear;
		gpu_light.constant = light.constant;

		gpu_point_lights.push_back( gpu_light );
	}

	gpu_scene_data.Upload( *gfx, &scene_data, sizeof( GpuSceneData ) );

	ssao_buffer.Upload( *gfx, &ssao_settings, sizeof( SSAOSettings ) );

	auto end = std::chrono::system_clock::now( );
	// convert to microseconds (integer), and then come back to miliseconds
	auto elapsed =
		std::chrono::duration_cast<std::chrono::microseconds>(end - start);
	stats.scene_update_time = elapsed.count( ) / 1000.f;
}
