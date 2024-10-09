#pragma once

#include <array>
#include <expected>

#include <graphics/descriptors.h>
#include "gbuffer.h"

class GfxDevice;

class Swapchain {
public:
	// -----------
	// Errors
	enum class Error {

	};

	template<typename T = void>
	using Result = std::expected<void, Error>;

	const static unsigned int FRAME_OVERLAP = 1;

	struct FrameData {
		VkCommandPool pool;
		VkCommandBuffer command_buffer;
		VkSemaphore swapchain_semaphore;
		VkSemaphore render_semaphore;
		VkFence fence;
		DeletionQueue deletion_queue;
		ImageID hdr_color;
		ImageID ssao;
		ImageID post_process_image;
		ImageID depth;
		GBuffer gbuffer;
	};

	VkSwapchainKHR swapchain;
	std::array<FrameData, FRAME_OVERLAP> frames;
	std::vector<VkImage> images;
	std::vector<VkImageView> views;
	VkFormat format;
	VkExtent2D extent;
	VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;

	uint64_t frame_number = 0;
	FrameData& getCurrentFrame( );

	Result<> init( GfxDevice* gfx, uint32_t width, uint32_t height );
	void cleanup( );

	Result<> recreate( uint32_t width, uint32_t height );
private:
	Result<> create( uint32_t width, uint32_t height );
	void createFrameImages( );
	void createGBuffers( );

	VkSampler linear = nullptr;
	GfxDevice* gfx = nullptr;
};
