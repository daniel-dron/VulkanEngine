#pragma once

#include <expected>
#include <vk_types.h>
#include "image_codex.h"
#include "swapchain.h"

class GfxDevice;

struct ImmediateExecutor {
	VkFence fence;
	VkCommandBuffer command_buffer;
	VkCommandPool pool;

	enum class Error {

	};

	template<typename T = void>
	using Result = std::expected<T, Error>;

	Result<> init( GfxDevice* gfx );
	void execute( GfxDevice* gfx, std::function<void( VkCommandBuffer )>&& func );
	void cleanup( GfxDevice* gfx );
};

class GfxDevice {
public:
	enum class Error {
		InstanceCreationFailed,
		PhysicalDeviceSelectionFailed,
		LogicalDeviceCreationFailed,
		GlobalAllocatorFailed,
	};

	struct GfxDeviceError {
		Error error;
	};

	template<typename T = void>
	using Result = std::expected<T, GfxDeviceError>;

	Result<> init( struct SDL_Window* window );
	void execute( std::function<void( VkCommandBuffer )>&& func );
	void cleanup( );

	VkInstance instance;
	VkPhysicalDevice chosen_gpu;
	VkDevice device;

	VkDebugUtilsMessengerEXT debug_messenger;

	VkQueue graphics_queue;
	uint32_t graphics_queue_family;

	ImmediateExecutor executor;
	VmaAllocator allocator;

	VkSurfaceKHR surface;
	::Swapchain swapchain;
	std::array<ImageID, ::Swapchain::FRAME_OVERLAP> images;

	ImageCodex image_codex;

private:
	Result<> initDevice( struct SDL_Window* window );
	Result<> initAllocator( );

	DeletionQueue deletion_queue;
};