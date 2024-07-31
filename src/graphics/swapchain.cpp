#include "swapchain.h"

#include "gfx_device.h"
#include "VkBootstrap.h"
#include "vk_types.h"
#include <vk_initializers.h>

Swapchain::FrameData& Swapchain::getCurrentFrame()
{
    return frames[frame_number % FRAME_OVERLAP];
}

Swapchain::Result<> Swapchain::init(GfxDevice* gfx, uint32_t width, uint32_t height)
{
    RETURN_IF_ERROR(create(gfx, width, height));

    extent = VkExtent2D{width, height};

	const VkCommandPoolCreateInfo command_pool_info =
		vkinit::command_pool_create_info(
			gfx->graphics_queue_family,
			VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
	for (int i = 0; i < FRAME_OVERLAP; i++) {
		VK_CHECK(vkCreateCommandPool(gfx->device, &command_pool_info, nullptr,
			&frames[i].pool));

		// default command buffer that will be used for rendering
		VkCommandBufferAllocateInfo cmd_alloc_info =
			vkinit::command_buffer_allocate_info(frames[i].pool, 1);
		VK_CHECK(vkAllocateCommandBuffers(gfx->device, &cmd_alloc_info,
			&frames[i].command_buffer));
	}

    auto fenceCreateInfo =
		vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
	auto semaphoreCreateInfo = vkinit::semaphore_create_info();

	for (int i = 0; i < FRAME_OVERLAP; i++) {
		VK_CHECK(vkCreateFence(gfx->device, &fenceCreateInfo, nullptr,
			&frames[i].fence));

		VK_CHECK(vkCreateSemaphore(gfx->device, &semaphoreCreateInfo, nullptr,
			&frames[i].render_semaphore));
		VK_CHECK(vkCreateSemaphore(gfx->device, &semaphoreCreateInfo, nullptr,
			&frames[i].swapchain_semaphore));
	}

    return {};
}

void Swapchain::cleanup(const GfxDevice* gfx)
{
    for (uint64_t i = 0; i < FRAME_OVERLAP; i++)
    {
        vkDestroyCommandPool(gfx->device, frames[i].pool, nullptr);

        // sync objects
        vkDestroyFence(gfx->device, frames[i].fence, nullptr);
        vkDestroySemaphore(gfx->device, frames[i].render_semaphore, nullptr);
        vkDestroySemaphore(gfx->device, frames[i].swapchain_semaphore, nullptr);

        frames[i].deletion_queue.flush();
		frames[i].frame_descriptors.destroy_pools(gfx->device);
    }

    vkDestroySwapchainKHR(gfx->device, swapchain, nullptr);

    for (const auto& view : views)
    {
        vkDestroyImageView(gfx->device, view, nullptr);
    }
}

Swapchain::Result<> Swapchain::recreate(GfxDevice* gfx, uint32_t width, uint32_t height)
{
    return {};
}

Swapchain::Result<> Swapchain::create(GfxDevice* gfx, uint32_t width, uint32_t height)
{
    vkb::SwapchainBuilder builder{gfx->chosen_gpu, gfx->device, gfx->surface};
    format = VK_FORMAT_R8G8B8A8_SRGB;

    auto swapchain_res = builder
                         .set_desired_format(VkSurfaceFormatKHR{
                             .format = format,
                             .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
                         })
                         .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
                         .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
                         .build();

    if (!swapchain_res)
    {
        return std::unexpected(Error{});
    }

    auto& bs_swapchain = swapchain_res.value();

    swapchain = bs_swapchain.swapchain;
    images = bs_swapchain.get_images().value();
    views = bs_swapchain.get_image_views().value();

    return {};
}
