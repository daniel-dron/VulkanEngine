#include "swapchain.h"

#include "gfx_device.h"
#include "VkBootstrap.h"
#include "vk_types.h"
#include <vk_initializers.h>

Swapchain::FrameData& Swapchain::getCurrentFrame( ) {
	return frames[frame_number % FRAME_OVERLAP];
}

Swapchain::Result<> Swapchain::init( GfxDevice* gfx, uint32_t width, uint32_t height ) {
	this->gfx = gfx;
	extent = VkExtent2D{ width, height };

	RETURN_IF_ERROR( create( width, height ) );

	const VkCommandPoolCreateInfo command_pool_info =
		vkinit::command_pool_create_info(
			gfx->graphics_queue_family,
			VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT );
	for ( int i = 0; i < FRAME_OVERLAP; i++ ) {
		VK_CHECK( vkCreateCommandPool( gfx->device, &command_pool_info, nullptr,
			&frames[i].pool ) );

		// default command buffer that will be used for rendering
		VkCommandBufferAllocateInfo cmd_alloc_info =
			vkinit::command_buffer_allocate_info( frames[i].pool, 1 );
		VK_CHECK( vkAllocateCommandBuffers( gfx->device, &cmd_alloc_info,
			&frames[i].command_buffer ) );
	}

	auto fenceCreateInfo =
		vkinit::fence_create_info( VK_FENCE_CREATE_SIGNALED_BIT );
	auto semaphoreCreateInfo = vkinit::semaphore_create_info( );

	for ( int i = 0; i < FRAME_OVERLAP; i++ ) {
		VK_CHECK( vkCreateFence( gfx->device, &fenceCreateInfo, nullptr,
			&frames[i].fence ) );

		VK_CHECK( vkCreateSemaphore( gfx->device, &semaphoreCreateInfo, nullptr,
			&frames[i].render_semaphore ) );
		VK_CHECK( vkCreateSemaphore( gfx->device, &semaphoreCreateInfo, nullptr,
			&frames[i].swapchain_semaphore ) );
	}

	return {};
}

void Swapchain::cleanup( ) {
	for ( uint64_t i = 0; i < FRAME_OVERLAP; i++ ) {
		vkDestroyCommandPool( gfx->device, frames[i].pool, nullptr );

		// sync objects
		vkDestroyFence( gfx->device, frames[i].fence, nullptr );
		vkDestroySemaphore( gfx->device, frames[i].render_semaphore, nullptr );
		vkDestroySemaphore( gfx->device, frames[i].swapchain_semaphore, nullptr );

		frames[i].deletion_queue.flush( );
		frames[i].frame_descriptors.destroy_pools( gfx->device );
	}

	vkDestroySwapchainKHR( gfx->device, swapchain, nullptr );

	for ( const auto& view : views ) {
		vkDestroyImageView( gfx->device, view, nullptr );
	}
}

Swapchain::Result<> Swapchain::recreate( uint32_t width, uint32_t height ) {

	auto old = swapchain;
	auto old_views = views;

	vkb::SwapchainBuilder builder{ gfx->chosen_gpu, gfx->device, gfx->surface };
	format = VK_FORMAT_R8G8B8A8_SRGB;

	auto swapchain_res = builder
		.set_desired_format( VkSurfaceFormatKHR{
			.format = format,
			.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
			} )
			.set_desired_present_mode( VK_PRESENT_MODE_FIFO_KHR )
		.add_image_usage_flags( VK_IMAGE_USAGE_TRANSFER_DST_BIT )
		.set_old_swapchain( swapchain )
		.build( );

	if ( !swapchain_res ) {
		return std::unexpected( Error{} );
	}

	auto& bs_swapchain = swapchain_res.value( );

	swapchain = bs_swapchain.swapchain;
	images = bs_swapchain.get_images( ).value( );
	views = bs_swapchain.get_image_views( ).value( );

	extent = VkExtent2D{ width, height };

	// ----------
	// Recreate and destroy left over primitives
	vkDestroySemaphore( gfx->device, getCurrentFrame( ).swapchain_semaphore, nullptr );
	const VkSemaphoreCreateInfo info = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		.pNext = nullptr,
	};
	VK_CHECK( vkCreateSemaphore( gfx->device, &info, nullptr, &getCurrentFrame( ).swapchain_semaphore ) );

	vkDestroySwapchainKHR( gfx->device, old, nullptr );
	for ( const auto& view : old_views ) {
		vkDestroyImageView( gfx->device, view, nullptr );
	}

	createFrameImages( );

	return {};
}

Swapchain::Result<> Swapchain::create( uint32_t width, uint32_t height ) {
	vkb::SwapchainBuilder builder{ gfx->chosen_gpu, gfx->device, gfx->surface };
	format = VK_FORMAT_R8G8B8A8_SRGB;

	auto swapchain_res = builder
		.set_desired_format( VkSurfaceFormatKHR{
			.format = format,
			.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
			} )
			.set_desired_present_mode( VK_PRESENT_MODE_FIFO_KHR )
		.add_image_usage_flags( VK_IMAGE_USAGE_TRANSFER_DST_BIT )
		.build( );

	if ( !swapchain_res ) {
		return std::unexpected( Error{} );
	}

	auto& bs_swapchain = swapchain_res.value( );

	swapchain = bs_swapchain.swapchain;
	images = bs_swapchain.get_images( ).value( );
	views = bs_swapchain.get_image_views( ).value( );

	createFrameImages( );

	return {};
}

	//auto& color_image = gfx->image_codex.getImage( gfx->swapchain.getCurrentFrame( ).color );
void Swapchain::createFrameImages( ) {
	const VkExtent3D draw_image_extent = {
		.width = extent.width, .height = extent.height, .depth = 1 };

	VkImageUsageFlags draw_image_usages{};
	draw_image_usages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	draw_image_usages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	draw_image_usages |= VK_IMAGE_USAGE_STORAGE_BIT;
	draw_image_usages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	draw_image_usages |= VK_IMAGE_USAGE_SAMPLED_BIT;

	VkImageUsageFlags depth_image_usages{};
	depth_image_usages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

	for ( auto& frame : frames ) {
		std::vector<unsigned char> empty_image_data;
		empty_image_data.resize( extent.width * extent.height * 8, 0 );
		frame.color = gfx->image_codex.loadImageFromData( "main draw image", empty_image_data.data( ), draw_image_extent,
			VK_FORMAT_R16G16B16A16_SFLOAT, draw_image_usages, false );

		empty_image_data.resize( extent.width * extent.height * 4, 0 );
		frame.depth = gfx->image_codex.loadImageFromData( "main depth image", empty_image_data.data( ), draw_image_extent,
			VK_FORMAT_D32_SFLOAT, depth_image_usages, false );
	}
}

void Swapchain::createImguiSet( VkSampler sampler ) {

	for ( auto& frame : frames ) {
		
	}
}
