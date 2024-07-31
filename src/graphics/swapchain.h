#pragma once

#include <array>
#include <expected>

#include "vk_descriptors.h"

class GfxDevice;

class Swapchain {
public:
    // -----------
    // Errors
    enum class Error
    {
        
    };

    template<typename T = void>
    using Result = std::expected<void, Error>;
    
    const static unsigned int FRAME_OVERLAP = 3;

    struct FrameData {
        VkCommandPool pool;
        VkCommandBuffer command_buffer;
        VkSemaphore swapchain_semaphore;
        VkSemaphore render_semaphore;
        VkFence fence;
        DeletionQueue deletion_queue;
	    DescriptorAllocatorGrowable frame_descriptors;
    };

    VkSwapchainKHR swapchain;
    std::array<FrameData, FRAME_OVERLAP> frames;
    std::vector<VkImage> images;
    std::vector<VkImageView> views;
    VkFormat format;
    VkExtent2D extent;

    uint64_t frame_number = 0;
	FrameData& getCurrentFrame();

    Result<> init(GfxDevice* gfx, uint32_t width, uint32_t height);
    void cleanup(const GfxDevice* gfx);
    
    Result<> recreate(GfxDevice* gfx, uint32_t width, uint32_t height);
private:
    Result<> create(GfxDevice* gfx, uint32_t width, uint32_t height);
};
