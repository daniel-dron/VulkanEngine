/******************************************************************************
******************************************************************************
**                                                                           **
**                             Twilight Engine                               **
**                                                                           **
**                  Copyright (c) 2024-present Daniel Dron                   **
**                                                                           **
**            This software is released under the MIT License.               **
**                 https://opensource.org/licenses/MIT                       **
**                                                                           **
******************************************************************************
******************************************************************************/

#pragma once

#include <array>
#include <expected>

#include "gbuffer.h"

class GfxDevice;

class Swapchain {
public:
	enum class Error { };

	template<typename T = void>
	using Result = std::expected<void, Error>;

    static constexpr unsigned int FrameOverlap = 1;

	struct FrameData {
		VkCommandPool pool;
		VkCommandBuffer commandBuffer;
		VkSemaphore swapchainSemaphore;
		VkSemaphore renderSemaphore;
		VkFence fence;
		DeletionQueue deletionQueue;
		ImageId hdrColor;
		ImageId ssao;
		ImageId postProcessImage;
		ImageId depth;
		GBuffer gBuffer;
	};

	VkSwapchainKHR swapchain;
	std::array<FrameData, FrameOverlap> frames;
	std::vector<VkImage> images;
	std::vector<VkImageView> views;
	VkFormat format;
	VkExtent2D extent;
	VkPresentModeKHR presentMode = VK_PRESENT_MODE_MAILBOX_KHR;

	uint64_t frameNumber = 0;
	FrameData& GetCurrentFrame( );

	Result<> Init( GfxDevice* gfx, uint32_t width, uint32_t height );
	void Cleanup( );

	Result<> Recreate( uint32_t width, uint32_t height );
private:
	Result<> Create( uint32_t width, uint32_t height );
	void CreateFrameImages( );
	void CreateGBuffers( );

	VkSampler m_linear = nullptr;
	GfxDevice* m_gfx = nullptr;
};
