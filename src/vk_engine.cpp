#include "vk_engine.h"

#include <SDL.h>
#include <SDL_vulkan.h>
#include <VkBootstrap.h>
#include <vk_images.h>
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
#include "glm/packing.hpp"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"
#include "vk_descriptors.h"

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#define TRACY_ENABLE
#include <glm/gtc/type_ptr.hpp>

#include "engine/input.h"
#include "imguizmo/ImGuizmo.h"
#include "tracy/TracyClient.cpp"
#include "tracy/tracy/Tracy.hpp"

VulkanEngine* loaded_engine = nullptr;
constexpr bool B_USE_VALIDATION_LAYERS = true;

VulkanEngine& VulkanEngine::get( ) { return *loaded_engine; }

void VulkanEngine::init( ) {
	// only one engine initialization is allowed with the application.
	assert( loaded_engine == nullptr );
	loaded_engine = this;

	initSdl( );

	initVulkan( );

	image_codex.init( this );
	initDefaultData( );

	initImgui( );

	EG_INPUT.init( );

	initScene( );

	const std::string structure_path = { "../../assets/sponza_scene.glb" };
	const auto structure_file = loadGltf( this, structure_path );
	assert( structure_file.has_value( ) );
	loaded_scenes["structure"] = *structure_file;

	// init camera
	fps_controller =
		std::make_unique<FirstPersonFlyingController>( &camera, 0.1f, 5.0f );
	camera_controller = fps_controller.get( );
	// camera.transform.set_position({0.0f, 0.0f, 100.0f});

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
	gfx->init( window );

	main_deletion_queue.flush( );

	initDrawImages( );
	initDescriptors( );
	initPipelines( );
}

void VulkanEngine::initDrawImages( ) {
	const VkExtent3D draw_image_extent = {
		.width = window_extent.width, .height = window_extent.height, .depth = 1 };

	draw_image.extent = draw_image_extent;
	draw_image.format = VK_FORMAT_R16G16B16A16_SFLOAT;

	VkImageUsageFlags draw_image_usages{};
	draw_image_usages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	draw_image_usages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	draw_image_usages |= VK_IMAGE_USAGE_STORAGE_BIT;
	draw_image_usages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	draw_image_usages |= VK_IMAGE_USAGE_SAMPLED_BIT;

	const VkImageCreateInfo rimg_info = vkinit::image_create_info(
		draw_image.format, draw_image_usages, draw_image_extent );

	// for the draw image, we want to allocate it from gpu local memory
	VmaAllocationCreateInfo rimg_allocinfo = {};
	rimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	rimg_allocinfo.requiredFlags =
		static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	vmaCreateImage( gfx->allocator, &rimg_info, &rimg_allocinfo, &draw_image.image,
		&draw_image.allocation, nullptr );

	const auto rview_info = vkinit::imageview_create_info(
		draw_image.format, draw_image.image, VK_IMAGE_ASPECT_COLOR_BIT );

	VK_CHECK( vkCreateImageView( gfx->device, &rview_info, nullptr, &draw_image.view ) );

	depth_image.format = VK_FORMAT_D32_SFLOAT;
	depth_image.extent = draw_image_extent;
	VkImageUsageFlags depth_image_usages{};
	depth_image_usages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

	const VkImageCreateInfo dimg_info = vkinit::image_create_info(
		depth_image.format, depth_image_usages, draw_image_extent );

	// allocate and create the image
	vmaCreateImage( gfx->allocator, &dimg_info, &rimg_allocinfo, &depth_image.image,
		&depth_image.allocation, nullptr );

	// build a image-view for the draw image to use for rendering
	const VkImageViewCreateInfo dview_info = vkinit::imageview_create_info(
		depth_image.format, depth_image.image, VK_IMAGE_ASPECT_DEPTH_BIT );

	VK_CHECK( vkCreateImageView( gfx->device, &dview_info, nullptr, &depth_image.view ) );

	// add to deletion queues
	main_deletion_queue.pushFunction( [&]( ) {
		vkDestroyImageView( gfx->device, draw_image.view, nullptr );
		vmaDestroyImage( gfx->allocator, draw_image.image, draw_image.allocation );

		vkDestroyImageView( gfx->device, depth_image.view, nullptr );
		vmaDestroyImage( gfx->allocator, depth_image.image, depth_image.allocation );
	} );
}

void VulkanEngine::initDescriptors( ) {
	std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes = {
		{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1} };

	global_descriptor_allocator.init( gfx->device, 1000, sizes );

	{
		DescriptorLayoutBuilder builder;
		builder.add_binding( 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE );
		draw_image_descriptor_layout =
			builder.build( gfx->device, VK_SHADER_STAGE_COMPUTE_BIT, nullptr );
	}

	draw_image_descriptors = global_descriptor_allocator.allocate(
		gfx->device, draw_image_descriptor_layout );

	DescriptorWriter writer;
	writer.write_image( 0, draw_image.view, VK_NULL_HANDLE,
		VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE );
	writer.update_set( gfx->device, draw_image_descriptors );

	for ( int i = 0; i < FRAME_OVERLAP; i++ ) {
		// create a descriptor pool
		std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> frame_sizes = {
			{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3},
			{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3},
			{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3},
			{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4},
		};

		gfx->swapchain.frames[i].frame_descriptors = DescriptorAllocatorGrowable{};
		gfx->swapchain.frames[i].frame_descriptors.init( gfx->device, 1000, frame_sizes );

		main_deletion_queue.pushFunction(
			[&, i]( ) { gfx->swapchain.frames[i].frame_descriptors.destroy_pools( gfx->device ); } );
	}

	//
	// init scene descriptors
	//
	{
		DescriptorLayoutBuilder builder;
		builder.add_binding( 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER );
		gpu_scene_data_descriptor_layout = builder.build(
			gfx->device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
			nullptr );
	}

	// make sure both the descriptor allocator and the new layout get cleaned up
	// properly
	main_deletion_queue.pushFunction( [&]( ) {
		global_descriptor_allocator.destroy_pools( gfx->device );

		vkDestroyDescriptorSetLayout( gfx->device, draw_image_descriptor_layout, nullptr );
	} );
}

void VulkanEngine::initPipelines( ) {
	// compute
	initBackgroundPipelines( );

	metal_rough_material.buildPipelines( this );
}

void VulkanEngine::initBackgroundPipelines( ) {
	VkPipelineLayoutCreateInfo computeLayout{};
	computeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	computeLayout.pNext = nullptr;
	computeLayout.pSetLayouts = &draw_image_descriptor_layout;
	computeLayout.setLayoutCount = 1;

	// push constants
	VkPushConstantRange pushConstant{};
	pushConstant.offset = 0;
	pushConstant.size = sizeof( ComputePushConstants );
	pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	computeLayout.pushConstantRangeCount = 1;
	computeLayout.pPushConstantRanges = &pushConstant;

	VK_CHECK( vkCreatePipelineLayout( gfx->device, &computeLayout, nullptr,
		&gradient_pipeline_layout ) );

	VkShaderModule gradientShader;
	if ( !vkutil::load_shader_module( "../../shaders/gradient_color.comp.spv",
		gfx->device, &gradientShader ) ) {
		fmt::println(
			"Error when building the compute shader [gradient_color.comp]" );
	}

	VkShaderModule skyShader;
	if ( !vkutil::load_shader_module( "../../shaders/sky.comp.spv", gfx->device,
		&skyShader ) ) {
		fmt::println( "Error when building the compute shader [sky.comp]" );
	}

	VkPipelineShaderStageCreateInfo stageinfo{};
	stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stageinfo.pNext = nullptr;
	stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stageinfo.module = gradientShader;
	stageinfo.pName = "main";

	VkComputePipelineCreateInfo computePipelineCreateInfo{};
	computePipelineCreateInfo.sType =
		VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	computePipelineCreateInfo.pNext = nullptr;
	computePipelineCreateInfo.layout = gradient_pipeline_layout;
	computePipelineCreateInfo.stage = stageinfo;

	ComputeEffect gradient;
	gradient.layout = gradient_pipeline_layout;
	gradient.name = "gradient";
	gradient.data = { .data1 = glm::vec4( 1, 0, 0, 1 ),
					 .data2 = glm::vec4( 0, 0, 1, 1 ) };

	VK_CHECK( vkCreateComputePipelines( gfx->device, VK_NULL_HANDLE, 1,
		&computePipelineCreateInfo, nullptr,
		&gradient.pipeline ) );

	ComputeEffect sky;
	sky.layout = gradient_pipeline_layout;
	sky.name = "sky";
	sky.data = { .data1 = glm::vec4( 0.1, 0.2, 0.4, 0.97 ) };

	VK_CHECK( vkCreateComputePipelines( gfx->device, VK_NULL_HANDLE, 1,
		&computePipelineCreateInfo, nullptr,
		&sky.pipeline ) );

	background_effects.push_back( gradient );
	background_effects.push_back( sky );

	vkDestroyShaderModule( gfx->device, gradientShader, nullptr );
	vkDestroyShaderModule( gfx->device, skyShader, nullptr );
	main_deletion_queue.pushFunction( [&, sky, gradient]( ) {
		vkDestroyPipelineLayout( gfx->device, gradient_pipeline_layout, nullptr );
		vkDestroyPipeline( gfx->device, sky.pipeline, nullptr );
		vkDestroyPipeline( gfx->device, gradient.pipeline, nullptr );
	} );
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

	viewport_set =
		ImGui_ImplVulkan_AddTexture( default_sampler_linear, draw_image.view,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL );

	// add the destroy the imgui created structures
	main_deletion_queue.pushFunction( [&, imguiPool]( ) {
		ImGui_ImplVulkan_Shutdown( );
		vkDestroyDescriptorPool( gfx->device, imguiPool, nullptr );
	} );
}

AllocatedImage VulkanEngine::createImage( VkExtent3D size, VkFormat format,
	VkImageUsageFlags usage,
	bool mipmapped ) {
	AllocatedImage newImage;
	newImage.format = format;
	newImage.extent = size;

	VkImageCreateInfo img_info = vkinit::image_create_info( format, usage, size );
	if ( mipmapped ) {
		img_info.mipLevels = static_cast<uint32_t>(std::floor(
			std::log2( std::max( size.width, size.height ) ) )) +
			1;
	}

	// always allocate images on dedicated GPU memory
	VmaAllocationCreateInfo allocinfo = {};
	allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	allocinfo.requiredFlags =
		VkMemoryPropertyFlags( VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );

	// allocate and create the image
	VK_CHECK( vmaCreateImage( gfx->allocator, &img_info, &allocinfo, &newImage.image,
		&newImage.allocation, nullptr ) );

	// if the format is a depth format, we will need to have it use the correct
	// aspect flag
	VkImageAspectFlags aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;
	if ( format == VK_FORMAT_D32_SFLOAT ) {
		aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT;
	}

	// build a image-view for the image
	VkImageViewCreateInfo view_info =
		vkinit::imageview_create_info( format, newImage.image, aspectFlag );
	view_info.subresourceRange.levelCount = img_info.mipLevels;

	VK_CHECK( vkCreateImageView( gfx->device, &view_info, nullptr, &newImage.view ) );

	return newImage;
}

AllocatedImage VulkanEngine::createImage( void* data, VkExtent3D size,
	VkFormat format,
	VkImageUsageFlags usage,
	bool mipmapped ) {
	size_t data_size = size.depth * size.width * size.height * 4;
	AllocatedBuffer uploadbuffer = gfx->allocate(
		data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, __FUNCTION__ );

	memcpy( uploadbuffer.info.pMappedData, data, data_size );

	AllocatedImage new_image = createImage(
		size, format,
		usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
		mipmapped );

	gfx->execute( [&]( VkCommandBuffer cmd ) {
		vkutil::transition_image( cmd, new_image.image, VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL );

		VkBufferImageCopy copyRegion = {};
		copyRegion.bufferOffset = 0;
		copyRegion.bufferRowLength = 0;
		copyRegion.bufferImageHeight = 0;

		copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copyRegion.imageSubresource.mipLevel = 0;
		copyRegion.imageSubresource.baseArrayLayer = 0;
		copyRegion.imageSubresource.layerCount = 1;
		copyRegion.imageExtent = size;

		// copy the buffer into the image
		vkCmdCopyBufferToImage( cmd, uploadbuffer.buffer, new_image.image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
			&copyRegion );

		if ( mipmapped ) {
			vkutil::generate_mipmaps(
				cmd, new_image.image,
				VkExtent2D{ new_image.extent.width, new_image.extent.height } );
		} else {
			vkutil::transition_image( cmd, new_image.image,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
		}
	} );
	gfx->free( uploadbuffer );
	return new_image;
}

void VulkanEngine::destroyImage( const GpuImage& img ) {
	vkDestroyImageView( gfx->device, img.view, nullptr );
	vmaDestroyImage( gfx->allocator, img.image, img.allocation );
}

GPUMeshBuffers VulkanEngine::uploadMesh( std::span<uint32_t> indices,
	std::span<Vertex> vertices ) {
	const size_t vertexBufferSize = vertices.size( ) * sizeof( Vertex );
	const size_t indexBufferSize = indices.size( ) * sizeof( uint32_t );

	GPUMeshBuffers newSurface;

	newSurface.vertexBuffer = gfx->allocate(
		vertexBufferSize,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VMA_MEMORY_USAGE_GPU_ONLY, "vertexBuffer" );

	// find the adress of the vertex buffer
	VkBufferDeviceAddressInfo deviceAddressInfo{
		.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
		.buffer = newSurface.vertexBuffer.buffer };
	newSurface.vertexBufferAddress =
		vkGetBufferDeviceAddress( gfx->device, &deviceAddressInfo );

	newSurface.indexBuffer = gfx->allocate(
		indexBufferSize,
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VMA_MEMORY_USAGE_GPU_ONLY, "indexBuffer" );

	//
	// staging phase.
	// create a temp buffer used to write the data from the cpu
	// then issue a gpu command to copy that data to the gpu only buffer
	//
	AllocatedBuffer staging =
		gfx->allocate( vertexBufferSize + indexBufferSize,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, __FUNCTION__ );

	// copy memory to gpu
	void* data = staging.allocation->GetMappedData( );
	memcpy( data, vertices.data( ), vertexBufferSize );
	memcpy( static_cast<char*>(data) + vertexBufferSize, indices.data( ),
		indexBufferSize );

	gfx->execute( [&]( const VkCommandBuffer cmd ) {
		VkBufferCopy vertex_copy;
		vertex_copy.dstOffset = 0;
		vertex_copy.srcOffset = 0;
		vertex_copy.size = vertexBufferSize;
		vkCmdCopyBuffer( cmd, staging.buffer, newSurface.vertexBuffer.buffer, 1,
			&vertex_copy );

		VkBufferCopy index_copy;
		index_copy.dstOffset = 0;
		index_copy.srcOffset = vertexBufferSize;
		index_copy.size = indexBufferSize;
		vkCmdCopyBuffer( cmd, staging.buffer, newSurface.indexBuffer.buffer, 1,
			&index_copy );
	} );

	gfx->free( staging );

	return newSurface;
}

void VulkanEngine::resizeSwapchain( uint32_t width, uint32_t height ) {
	// vkDeviceWaitIdle(gfx->device);
	//
	// window_extent.width = width;
	// window_extent.height = height;
	//
	// // save to delete them after creating the swap chain
	// auto old = swapchain;
	// auto oldImagesViews = swapchain_image_views;
	//
	// createSwapchain(window_extent.width, window_extent.height, swapchain);
	// // recreate the swapchain semaphore since it has already been signaled by the
	// // vkAcquireNextImageKHR
	// vkDestroySemaphore(gfx->device, getCurrentFrame().swapchain_semaphore, nullptr);
	// auto semaphoreCreateInfo = vkinit::semaphore_create_info();
	// VK_CHECK(vkCreateSemaphore(gfx->device, &semaphoreCreateInfo, nullptr,
	// 	&getCurrentFrame().swapchain_semaphore));
	//
	// vkDestroySwapchainKHR(gfx->device, old, nullptr);
	// for (unsigned int i = 0; i < oldImagesViews.size(); i++) {
	// 	vkDestroyImageView(gfx->device, oldImagesViews.at(i), nullptr);
	// }
}

void VulkanEngine::cleanup( ) {
	if ( is_initialized ) {
		// wait for gpu work to finish
		vkDeviceWaitIdle( gfx->device );

		loaded_scenes.clear( );

		vkDestroyDescriptorSetLayout( gfx->device, gpu_scene_data_descriptor_layout,
			nullptr );

		metal_rough_material.clearResources( gfx->device );

		main_deletion_queue.flush( );

		image_codex.cleanup( );
		gfx->cleanup( );

		SDL_DestroyWindow( window );
	}

	// clear engine pointer
	loaded_engine = nullptr;
}

void VulkanEngine::draw( ) {
	ZoneScopedN( "draw" );
	updateScene( );

	uint32_t swapchainImageIndex;
	{
		ZoneScopedN( "vsync" );
		// wait for last frame rendering phase. 1 sec timeout
		VK_CHECK( vkWaitForFences( gfx->device, 1, &gfx->swapchain.getCurrentFrame( ).fence, true,
			1000000000 ) );

		gfx->swapchain.getCurrentFrame( ).deletion_queue.flush( );
		gfx->swapchain.getCurrentFrame( ).frame_descriptors.clear_pools( gfx->device );

		VkResult e = (vkAcquireNextImageKHR( gfx->device, gfx->swapchain.swapchain, 1000000000,
			gfx->swapchain.getCurrentFrame( ).swapchain_semaphore,
			0, &swapchainImageIndex ));
		if ( e != VK_SUCCESS ) {
			return;
		}

		// Reset after acquire the next image from the swapchain
		// Incase of error, this fence would never get passed to the queue, thus
		// never triggering leaving us with a timeout next time we wait for fence
		VK_CHECK( vkResetFences( gfx->device, 1, &gfx->swapchain.getCurrentFrame( ).fence ) );
	}

	draw_extent.height = static_cast<uint32_t>(
		std::min( gfx->swapchain.extent.height, draw_image.extent.height ) *
		render_scale);
	draw_extent.width = static_cast<uint32_t>(
		std::min( gfx->swapchain.extent.width, draw_image.extent.width ) * render_scale);

	//
	// commands
	//
	auto cmd = gfx->swapchain.getCurrentFrame( ).command_buffer;

	// we can safely reset the command buffer because we waited for the render
	// fence
	VK_CHECK( vkResetCommandBuffer( cmd, 0 ) );

	// begin recording. will only use this command buffer once
	auto cmdBeginInfo = vkinit::command_buffer_begin_info(
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT );
	VK_CHECK( vkBeginCommandBuffer( cmd, &cmdBeginInfo ) );

	// transition our main draw image into general layout so it can
	// be written into
	vkutil::transition_image( cmd, draw_image.image, VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_GENERAL );

	drawBackground( cmd );

	vkutil::transition_image( cmd, draw_image.image, VK_IMAGE_LAYOUT_GENERAL,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL );
	vkutil::transition_image( cmd, depth_image.image, VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL );

	drawGeometry( cmd );

	// transform drawImg into source layout
	// transform swapchain img into dst layout
	vkutil::transition_image( cmd, draw_image.image,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL );
	vkutil::transition_image( cmd, gfx->swapchain.images[swapchainImageIndex],
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL );

	vkutil::copy_image_to_image( cmd, draw_image.image,
		gfx->swapchain.images[swapchainImageIndex],
		draw_extent, gfx->swapchain.extent );

	// transition swapchain to present
	vkutil::transition_image( cmd, gfx->swapchain.images[swapchainImageIndex],
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL );

	// draw imgui into the swapchain image
	drawImgui( cmd, gfx->swapchain.views[swapchainImageIndex] );

	// set swapchain image layout to Present so we can draw it
	vkutil::transition_image( cmd, gfx->swapchain.images[swapchainImageIndex],
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR );

	VK_CHECK( vkEndCommandBuffer( cmd ) );

	//
	// send commands
	//

	// wait on _swapchainSemaphore. signaled when the swap chain is ready
	// wait on _renderSemaphore. signaled when rendering has finished
	auto cmdinfo = vkinit::command_buffer_submit_info( cmd );

	auto waitInfo = vkinit::semaphore_submit_info(
		VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
		gfx->swapchain.getCurrentFrame( ).swapchain_semaphore );
	auto signalInfo = vkinit::semaphore_submit_info(
		VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, gfx->swapchain.getCurrentFrame( ).render_semaphore );

	auto submit = vkinit::submit_info( &cmdinfo, &signalInfo, &waitInfo );

	// submit command buffer and execute it
	// _renderFence will now block until the commands finish
	VK_CHECK( vkQueueSubmit2( gfx->graphics_queue, 1, &submit,
		gfx->swapchain.getCurrentFrame( ).fence ) );

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
	frame_number++;
}

void VulkanEngine::drawBackground( VkCommandBuffer cmd ) {
	ZoneScopedN( "draw_background" );
	auto& [name, pipeline, layout, data] =
		background_effects[current_background_effect];

	vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline );

	// bind the descriptor set containing the draw image for the compute pipeline
	vkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
		gradient_pipeline_layout, 0, 1,
		&draw_image_descriptors, 0, nullptr );

	vkCmdPushConstants( cmd, gradient_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
		0, sizeof( ComputePushConstants ), &data );

	// execute the compute pipeline dispatch. We are using 16x16 workgroup size so
	// we need to divide by it
	vkCmdDispatch( cmd, static_cast<uint32_t>(::ceil( draw_extent.width / 16.0 )),
		static_cast<uint32_t>(std::ceil( draw_extent.height / 16.0 )), 1 );
}

// TODO: breaking at certain angles
static bool is_visible( const RenderObject& obj, const glm::mat4& viewproj ) {
	constexpr std::array<glm::vec3, 8> corners{
		glm::vec3{1, 1, 1},   glm::vec3{1, 1, -1},   glm::vec3{1, -1, 1},
		glm::vec3{1, -1, -1}, glm::vec3{-1, 1, 1},   glm::vec3{-1, 1, -1},
		glm::vec3{-1, -1, 1}, glm::vec3{-1, -1, -1},
	};

	const glm::mat4 matrix = viewproj * obj.transform;

	glm::vec3 min = { 1.5, 1.5, 1.5 };
	glm::vec3 max = { -1.5, -1.5, -1.5 };

	for ( int c = 0; c < 8; c++ ) {
		// project each corner into clip space
		glm::vec4 v =
			matrix *
			glm::vec4( obj.bounds.origin + (corners[c] * obj.bounds.extents), 1.f );

		// perspective correction
		v.x = v.x / v.w;
		v.y = v.y / v.w;
		v.z = v.z / v.w;

		min = glm::min( glm::vec3{ v.x, v.y, v.z }, min );
		max = glm::max( glm::vec3{ v.x, v.y, v.z }, max );
	}

	// check the clip space box is within the view
	if ( min.z > 1.f || max.z < 0.f || min.x > 1.f || max.x < -1.f ||
		min.y > 1.f || max.y < -1.f ) {
		return false;
	} else {
		return true;
	}
}

void VulkanEngine::drawGeometry( VkCommandBuffer cmd ) {
	ZoneScopedN( "draw_geometry" );

	// reset counters
	stats.drawcall_count = 0;
	stats.triangle_count = 0;
	// begin clock
	auto start = std::chrono::system_clock::now( );

	//
	// sort opaque surfaces by material and mesh
	//
	std::vector<uint32_t> opaque_draws;
	opaque_draws.reserve( main_draw_context.opaque_surfaces.size( ) );
	{
		ZoneScopedN( "order" );
		for ( uint32_t i = 0; i < main_draw_context.opaque_surfaces.size( ); i++ ) {
			if ( renderer_options.frustum ) {
				auto& viewproj = scene_data.viewproj;
				if ( is_visible( main_draw_context.opaque_surfaces[i], viewproj ) ) {
					opaque_draws.push_back( i );
				}
			} else {
				opaque_draws.push_back( i );
			}
		}

		std::ranges::sort( opaque_draws, [&]( const auto& i_a, const auto& i_b ) {
			const RenderObject& a = main_draw_context.opaque_surfaces[i_a];
			const RenderObject& b = main_draw_context.opaque_surfaces[i_b];

			if ( a.material == b.material ) {
				return a.index_buffer < b.index_buffer;
			} else {
				return a.material < b.material;
			}
		} );
	}

	// ----------
	// scene buffers setup
	auto gpuSceneDataBuffer =
		gfx->allocate( sizeof( GpuSceneData ), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VMA_MEMORY_USAGE_CPU_TO_GPU, "drawGeometry" );

	gfx->swapchain.getCurrentFrame( ).deletion_queue.pushFunction( [=, this]( ) {
		gfx->free( gpuSceneDataBuffer );
	} );

	// write to the buffer
	auto scene_uniform_data = static_cast<GpuSceneData*>(
		gpuSceneDataBuffer.allocation->GetMappedData( ));
	*scene_uniform_data = scene_data;

	// create a descriptor set that binds that buffer and update it
	VkDescriptorSet global_descriptor =
		gfx->swapchain.getCurrentFrame( ).frame_descriptors.allocate(
			gfx->device, gpu_scene_data_descriptor_layout );

	DescriptorWriter writer;
	writer.write_buffer( 0, gpuSceneDataBuffer.buffer, sizeof( GpuSceneData ), 0,
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER );
	writer.update_set( gfx->device, global_descriptor );

	// -----------
	// begin render frame
	{
		VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(
			draw_image.view, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL );
		VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(
			depth_image.view, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL );

		VkRenderingInfo render_info =
			vkinit::rendering_info( draw_extent, &colorAttachment, &depthAttachment );
		vkCmdBeginRendering( cmd, &render_info );
	}

	MaterialPipeline* lastPipeline = nullptr;
	MaterialInstance* lastMaterial = nullptr;
	VkBuffer lastIndexBuffer = VK_NULL_HANDLE;

	auto draw = [&]( const RenderObject& r ) {
		if ( r.material != lastMaterial ) {
			lastMaterial = r.material;

			if ( r.material->pipeline != lastPipeline ) {
				lastPipeline = r.material->pipeline;

				if ( renderer_options.wireframe ) {
					vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
						metal_rough_material.wireframe_pipeline.pipeline );
				} else {
					vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
						r.material->pipeline->pipeline );
				}
				vkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
					r.material->pipeline->layout, 0, 1,
					&global_descriptor, 0, nullptr );

				VkViewport viewport;
				viewport.x = 0;
				viewport.y = 0;
				viewport.width = static_cast<float>(draw_extent.width);
				viewport.height = static_cast<float>(draw_extent.height);
				viewport.minDepth = 0.0f;
				viewport.maxDepth = 1.0f;

				vkCmdSetViewport( cmd, 0, 1, &viewport );

				VkRect2D scissor;
				scissor.offset.x = 0;
				scissor.offset.y = 0;
				scissor.extent.width = draw_extent.width;
				scissor.extent.height = draw_extent.height;

				vkCmdSetScissor( cmd, 0, 1, &scissor );
			}

			vkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
				r.material->pipeline->layout, 1, 1,
				&r.material->materialSet, 0, nullptr );
		}

		if ( r.index_buffer != lastIndexBuffer ) {
			lastIndexBuffer = r.index_buffer;
			vkCmdBindIndexBuffer( cmd, r.index_buffer, 0, VK_INDEX_TYPE_UINT32 );
		}

		// calculate final mesh matrix
		GPUDrawPushConstants push_constants;
		push_constants.worldMatrix = r.transform;
		push_constants.vertexBuffer = r.vertex_buffer_address;

		vkCmdPushConstants( cmd, r.material->pipeline->layout,
			VK_SHADER_STAGE_VERTEX_BIT, 0,
			sizeof( GPUDrawPushConstants ), &push_constants );

		vkCmdDrawIndexed( cmd, r.index_count, 1, r.first_index, 0, 0 );

		// stats
		stats.drawcall_count++;
		stats.triangle_count += r.index_count / 3;
	};

	{
		ZoneScopedN( "render" );
		for ( auto& r : opaque_draws ) {
			draw( main_draw_context.opaque_surfaces[r] );
		}

		for ( auto& r : main_draw_context.transparent_surfaces ) {
			draw( r );
		}
	}

	vkCmdEndRendering( cmd );

	auto end = std::chrono::system_clock::now( );

	// convert to microseconds (integer), and then come back to miliseconds
	auto elapsed =
		std::chrono::duration_cast<std::chrono::microseconds>(end - start);
	stats.mesh_draw_time = elapsed.count( ) / 1000.f;
}

void VulkanEngine::initDefaultData( ) { initImages( ); }

void VulkanEngine::initImages( ) {
	// 3 default textures, white, grey, black. 1 pixel each
	uint32_t white = glm::packUnorm4x8( glm::vec4( 1, 1, 1, 1 ) );
	white_image = image_codex.loadImageFromData( "debug_white_img", (void*)&white, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, false );

	uint32_t grey = glm::packUnorm4x8( glm::vec4( 0.66f, 0.66f, 0.66f, 1 ) );
	grey_image = image_codex.loadImageFromData( "debug_grey_img", (void*)&grey, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, false );

	uint32_t black = glm::packUnorm4x8( glm::vec4( 0, 0, 0, 0 ) );
	black_image = image_codex.loadImageFromData( "debug_black_img", (void*)&white, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, false );

	// checkerboard image
	uint32_t magenta = glm::packUnorm4x8( glm::vec4( 1, 0, 1, 1 ) );
	std::array<uint32_t, 16 * 16> pixels;  // for 16x16 checkerboard texture
	for ( int x = 0; x < 16; x++ ) {
		for ( int y = 0; y < 16; y++ ) {
			pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
		}
	}
	error_checkerboard_image = image_codex.loadImageFromData( "debug_checkboard_img", (void*)&white, VkExtent3D{ 16, 16, 1 }, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, false );

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
	scene_data.ambient_light_color = vec3( 1.0f, 1.0f, 1.0f );
	scene_data.ambient_light_factor = 1.0f;

	scene_data.fog_color = vec4( 1.0f, 0.0f, 0.0f, 1.0f );
	scene_data.fog_end = 20.0f;
	scene_data.fog_start = 1.0f;

	PointLight light = {};
	light.transform.set_position( vec3( 0.0f, 2.0f, 0.0f ) );
	light.color = vec4( 1.0f, 0.0f, 1.0f, 1000.0f );
	light.diffuse = 0.1f;
	light.specular = 1.0f;
	light.radius = 10.0f;

	point_lights.push_back( light );
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

Node* selected_node = nullptr;

void VulkanEngine::run( ) {
	bool bQuit = false;

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

		camera_controller->update( 1.0f / 165.0f );

		// do not draw if we are minimized
		if ( stop_rendering ) {
			// throttle the speed to avoid the endless spinning
			std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
			continue;
		}

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

			if ( ImGui::Begin( "Viewport", 0, ImGuiWindowFlags_NoScrollbar ) ) {
				ImGui::Image( (ImTextureID)(viewport_set),
					ImGui::GetWindowContentRegionMax( ) );

				// ----------
				// guizmos
				if ( selected_node != nullptr ) {
					ImGuizmo::SetOrthographic( false );
					ImGuizmo::SetDrawlist( );
					ImGuizmo::SetRect( ImGui::GetWindowPos( ).x, ImGui::GetWindowPos( ).y,
						(float)ImGui::GetWindowWidth( ),
						(float)ImGui::GetWindowHeight( ) );
					auto camera_view = scene_data.view;
					auto camera_proj = scene_data.proj;
					camera_proj[1][1] *= -1;

					auto& tc = selected_node->worldTransform;
					ImGuizmo::Manipulate( glm::value_ptr( camera_view ),
						glm::value_ptr( camera_proj ),
						ImGuizmo::OPERATION::UNIVERSAL,
						ImGuizmo::MODE::LOCAL, glm::value_ptr( tc ) );
				}
			}
			ImGui::End( );

			if ( ImGui::Begin( "Scene" ) ) {
				drawSceneHierarchy( );
			}
			ImGui::End( );

			if ( EG_INPUT.was_key_pressed( EG_KEY::Z ) ) {
				ImGui::OpenPopup( "Viewport Context" );
			}
			if ( ImGui::BeginPopup( "Viewport Context" ) ) {
				ImGui::Checkbox( "Wireframe", &renderer_options.wireframe );
				ImGui::Checkbox( "Frustum Culling", &renderer_options.frustum );
				ImGui::EndPopup( );
			}

			if ( ImGui::Begin( "Settings" ) ) {
				if ( selected_node != nullptr ) {
					ImGui::SeparatorText( selected_node->name.c_str( ) );
					MeshNode* node = dynamic_cast<MeshNode*>(selected_node);
					if ( node ) {
						ImGui::TextColored( ImVec4( 0.7f, 0.2f, 0.5f, 1.0f ), "Mesh: %s",
							node->mesh->name.c_str( ) );
						static ImGuiTableFlags flags =
							ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg |
							ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable |
							ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable;

						// material data
						auto idx = 0;
						for ( auto& surface : node->mesh->surfaces ) {
							if ( ImGui::CollapsingHeader(
								std::format( "Surface {}", idx ).c_str( ) ) ) {
								ImGui::InputText( "Name", (char*)surface.material->name.c_str( ),
									surface.material->name.size( ),
									ImGuiInputTextFlags_ReadOnly );

								std::string pipeline_name;
								switch ( surface.material->data.passType ) {
								case MaterialPass::MainColor:
									pipeline_name = "Opaque Material";
									break;
								case MaterialPass::Transparent:
									pipeline_name = "Transparent Material";
									break;
								default:
									pipeline_name = "Other";
								}

								ImGui::InputText( "Pipeline", (char*)pipeline_name.c_str( ),
									pipeline_name.size( ),
									ImGuiInputTextFlags_ReadOnly );

								// albedo
								if ( ImGui::CollapsingHeader( "Albedo" ) ) {
									ImGui::Image( (ImTextureID)(
										surface.material->debug_sets.base_color_set),
										ImVec2( 200, 200 ) );
								}

								// metal roughness
								if ( ImGui::CollapsingHeader( "Metal Roughness" ) ) {
									ImGui::Image(
										(ImTextureID)(
											surface.material->debug_sets.metal_roughness_set),
										ImVec2( 200, 200 ) );
								}

								// normal map
								if ( ImGui::CollapsingHeader( "Normal" ) ) {
									ImGui::Image( (ImTextureID)(
										surface.material->debug_sets.normal_map_set),
										ImVec2( 200, 200 ) );
								}
							}
						}
					}
				}
				ImGui::SeparatorText( "Camera 3D" );
				camera.draw_debug( );

				ImGui::SeparatorText( "Camera Controller" );
				camera_controller->draw_debug( );

				ImGui::SeparatorText( "Light" );
				ImGui::ColorEdit3( "Ambient Color", &scene_data.ambient_light_color.x );
				ImGui::DragFloat( "Ambient Diffuse", &scene_data.ambient_light_factor,
					0.01f, 0.0f, 1.0f );

				ImGui::SeparatorText( "Point Lights" );
				for ( auto i = 0u; i < point_lights.size( ); i++ ) {
					if ( ImGui::CollapsingHeader(
						std::format( "Point Light {}", i ).c_str( ) ) ) {
						ImGui::PushID( i );
						ImGui::ColorEdit3( "Color", &point_lights.at( i ).color.x );

						auto pos = point_lights.at( i ).transform.get_position( );
						ImGui::DragFloat3( "Pos", &pos.x, 0.1f );
						point_lights.at( i ).transform.set_position( pos );

						ImGui::DragFloat( "Diffuse", &point_lights.at( i ).diffuse, 0.01f,
							0.0f, 1.0f );
						ImGui::DragFloat( "Specular", &point_lights.at( i ).specular, 0.01f,
							0.0f, 1.0f );
						ImGui::DragFloat( "Radius", &point_lights.at( i ).radius, 0.1f );
						ImGui::PopID( );
					}
				}

				ImGui::SeparatorText( "Fog" );
				ImGui::ColorEdit4( "Fog Color", &scene_data.fog_color.x );
				ImGui::DragFloat( "Start", &scene_data.fog_start, 0.1f, 0.1f );
				ImGui::DragFloat( "End", &scene_data.fog_end, 0.1f,
					scene_data.fog_start + 0.1f );

				ImGui::SeparatorText( "Background" );
				ImGui::SliderFloat( "Render Scale", &render_scale, 0.3f, 1.f );
				ComputeEffect& selected = background_effects[current_background_effect];
				ImGui::Text( "Selected effect: %s", selected.name );
				ImGui::SliderInt( "Effect Index", &current_background_effect, 0,
					(int)background_effects.size( ) - 1 );
				ImGui::InputFloat4( "data1", (float*)&selected.data.data1 );
				ImGui::InputFloat4( "data2", (float*)&selected.data.data2 );
				ImGui::InputFloat4( "data3", (float*)&selected.data.data3 );
				ImGui::InputFloat4( "data4", (float*)&selected.data.data4 );
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
	}
}

void VulkanEngine::drawImgui( VkCommandBuffer cmd,
	VkImageView target_image_view ) {
	VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(
		target_image_view, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL );
	VkRenderingInfo renderInfo =
		vkinit::rendering_info( gfx->swapchain.extent, &colorAttachment, nullptr );

	vkCmdBeginRendering( cmd, &renderInfo );

	ImGui_ImplVulkan_RenderDrawData( ImGui::GetDrawData( ), cmd );

	vkCmdEndRendering( cmd );
}

void VulkanEngine::updateScene( ) {
	ZoneScopedN( "update_scene" );
	auto start = std::chrono::system_clock::now( );

	main_draw_context.opaque_surfaces.clear( );
	main_draw_context.transparent_surfaces.clear( );

	loaded_scenes["structure"]->Draw( glm::mat4{ 1.f }, main_draw_context );

	scene_data.view = camera.get_view_matrix( );
	// camera projection
	scene_data.proj = glm::perspective(
		glm::radians( 70.f ),
		(float)window_extent.width / (float)window_extent.height, 10000.f, 0.1f );

	scene_data.viewproj = scene_data.proj * scene_data.view;

	scene_data.camera_position = camera.transform.get_position( );

	// point lights
	scene_data.number_of_lights = static_cast<int>(point_lights.size( ));
	for ( size_t i = 0; i < point_lights.size( ); i++ ) {
		auto& light = scene_data.point_lights[i];
		light.position = point_lights[i].transform.get_position( );
		light.radius = point_lights[i].radius;
		light.color = point_lights[i].color;
		light.diffuse = point_lights[i].diffuse;
		light.specular = point_lights[i].specular;
	}

	auto end = std::chrono::system_clock::now( );
	// convert to microseconds (integer), and then come back to miliseconds
	auto elapsed =
		std::chrono::duration_cast<std::chrono::microseconds>(end - start);
	stats.scene_update_time = elapsed.count( ) / 1000.f;
}

void drawSceneNode( Node* node ) {
	MeshNode* mesh_node = dynamic_cast<MeshNode*>(node);

	if ( mesh_node ) {
		if ( ImGui::Selectable( node->name.c_str( ), selected_node == node ) ) {
			selected_node = node;
		}
	} else {
		ImGui::SetNextItemOpen( true, ImGuiCond_Once );
		if ( ImGui::TreeNode( node, node->name.c_str( ) ) ) {
			for ( auto& n : node->children ) {
				drawSceneNode( n.get( ) );
			}
			ImGui::TreePop( );
		}
	}
}

void VulkanEngine::drawSceneHierarchy( ) {
	for ( auto& [k, v] : loaded_scenes ) {
		ImGui::SetNextItemOpen( true, ImGuiCond_Once );
		if ( ImGui::TreeNode( k.c_str( ) ) ) {
			for ( auto& root : v->top_nodes ) {
				drawSceneNode( root.get( ) );
			}
			ImGui::TreePop( );
		}
	}
}

void GltfMetallicRoughness::buildPipelines( VulkanEngine* engine ) {
	VkShaderModule meshFragShader;
	if ( !vkutil::load_shader_module( "../../shaders/mesh.frag.spv", engine->gfx->device,
		&meshFragShader ) ) {
		fmt::println(
			"Error when building GLTFMetallic_Roughness fragment shader "
			"[mesh.frag.spv]" );
	}

	VkShaderModule meshVertShader;
	if ( !vkutil::load_shader_module( "../../shaders/mesh.vert.spv", engine->gfx->device,
		&meshVertShader ) ) {
		fmt::println(
			"Error when building GLTFMetallic_Roughness vert shader "
			"[mesh.vert.spv]" );
	}

	VkPushConstantRange matrixRange{};
	matrixRange.offset = 0;
	matrixRange.size = sizeof( GPUDrawPushConstants );
	matrixRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	// descriptor for set 1 (factors and textures for PBR)
	DescriptorLayoutBuilder layoutBuilder;
	layoutBuilder.add_binding( 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER );
	layoutBuilder.add_binding( 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER );
	layoutBuilder.add_binding( 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER );
	layoutBuilder.add_binding( 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER );
	material_layout = layoutBuilder.build(
		engine->gfx->device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		nullptr );

	VkDescriptorSetLayout layouts[] = { engine->gpu_scene_data_descriptor_layout,
									   material_layout };

	VkPipelineLayoutCreateInfo meshLayoutInfo =
		vkinit::pipeline_layout_create_info( );
	meshLayoutInfo.pSetLayouts = layouts;
	meshLayoutInfo.setLayoutCount = 2;
	meshLayoutInfo.pPushConstantRanges = &matrixRange;
	meshLayoutInfo.pushConstantRangeCount = 1;
	VkPipelineLayout newLayout;
	VK_CHECK( vkCreatePipelineLayout( engine->gfx->device, &meshLayoutInfo, nullptr,
		&newLayout ) );

	opaque_pipeline.layout = newLayout;
	transparent_pipeline.layout = newLayout;
	wireframe_pipeline.layout = newLayout;

	vkutil::PipelineBuilder pipelineBuilder;
	pipelineBuilder.set_shaders( meshVertShader, meshFragShader );
	pipelineBuilder.set_input_topology( VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST );
	pipelineBuilder.set_polygon_mode( VK_POLYGON_MODE_FILL );
	pipelineBuilder.set_cull_mode( VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE );
	pipelineBuilder.set_multisampling_none( );
	pipelineBuilder.disable_blending( );
	pipelineBuilder.enable_depthtest( true, VK_COMPARE_OP_GREATER_OR_EQUAL );

	// format
	pipelineBuilder.set_color_attachment_format( engine->draw_image.format );
	pipelineBuilder.set_depth_format( engine->depth_image.format );
	pipelineBuilder._pipelineLayout = newLayout;

	// create opaque pipeline
	opaque_pipeline.pipeline = pipelineBuilder.build_pipeline( engine->gfx->device );

	// create transparent pipeline
	pipelineBuilder.enable_blending_additive( );
	// do depthtest but dont write to depth texture
	pipelineBuilder.enable_depthtest( false, VK_COMPARE_OP_GREATER_OR_EQUAL );
	transparent_pipeline.pipeline =
		pipelineBuilder.build_pipeline( engine->gfx->device );

	pipelineBuilder.disable_blending( );
	pipelineBuilder.set_polygon_mode( VK_POLYGON_MODE_LINE );
	wireframe_pipeline.pipeline = pipelineBuilder.build_pipeline( engine->gfx->device );

	vkDestroyShaderModule( engine->gfx->device, meshFragShader, nullptr );
	vkDestroyShaderModule( engine->gfx->device, meshVertShader, nullptr );
}

void GltfMetallicRoughness::clearResources( VkDevice device ) {
	vkDestroyPipelineLayout( device, opaque_pipeline.layout, nullptr );
	vkDestroyDescriptorSetLayout( device, material_layout, nullptr );

	vkDestroyPipeline( device, opaque_pipeline.pipeline, nullptr );
	vkDestroyPipeline( device, transparent_pipeline.pipeline, nullptr );
	vkDestroyPipeline( device, wireframe_pipeline.pipeline, nullptr );
}

MaterialInstance GltfMetallicRoughness::writeMaterial(
	VulkanEngine* engine, MaterialPass pass, const MaterialResources& resources,
	DescriptorAllocatorGrowable& descriptor_allocator ) {
	MaterialInstance matData;
	matData.passType = pass;
	if ( pass == MaterialPass::Transparent ) {
		matData.pipeline = &transparent_pipeline;
	} else {
		matData.pipeline = &opaque_pipeline;
	}

	matData.materialSet = descriptor_allocator.allocate( engine->gfx->device, material_layout );

	auto& color = engine->image_codex.getImage( resources.color_image );
	auto& metal_rough = engine->image_codex.getImage( resources.metal_rough_image );
	auto& normal = engine->image_codex.getImage( resources.normal_map );

	writer.clear( );
	writer.write_buffer( 0, resources.data_buffer, sizeof( MaterialConstants ),
		resources.data_buffer_offset,
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER );
	writer.write_image( 1, color.view, resources.color_sampler,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER );
	writer.write_image( 2, metal_rough.view,
		resources.metal_rough_sampler,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER );
	writer.write_image( 3, normal.view, resources.normal_sampler,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER );

	writer.update_set( engine->gfx->device, matData.materialSet );

	return matData;
}

void MeshNode::Draw( const glm::mat4& top_matrix, DrawContext& ctx ) {
	glm::mat4 nodeMatrix = top_matrix * worldTransform;

	for ( auto& s : mesh->surfaces ) {
		RenderObject def;
		def.index_count = s.count;
		def.first_index = s.start_index;
		def.index_buffer = mesh->mesh_buffers.indexBuffer.buffer;
		def.material = &s.material->data;
		def.bounds = s.bounds;
		def.transform = nodeMatrix;
		def.vertex_buffer_address = mesh->mesh_buffers.vertexBufferAddress;

		if ( s.material->data.passType == MaterialPass::Transparent ) {
			ctx.transparent_surfaces.push_back( def );
		} else {
			ctx.opaque_surfaces.push_back( def );
		}
	}

	Node::Draw( top_matrix, ctx );
}
