#pragma once

#include <expected>
#include <vk_types.h>
#include <mutex>
#include "image_codex.h"
#include "material_codex.h"
#include "mesh_codex.h"
#include "swapchain.h"

class GfxDevice;

#ifdef ENABLE_DEBUG_UTILS
#define START_LABEL(cmd, name, color) Debug::StartLabel(cmd, name, color)
#define END_LABEL(cmd) Debug::EndLabel(cmd)
#else
#define START_LABEL(cmd, name, color)
#define END_LABEL
#endif

namespace Debug {
	extern PFN_vkCmdBeginDebugUtilsLabelEXT vkCmdBeginDebugUtilsLabelEXT_ptr;
	extern PFN_vkCmdEndDebugUtilsLabelEXT vkCmdEndDebugUtilsLabelEXT_ptr;
	extern PFN_vkCmdInsertDebugUtilsLabelEXT vkCmdInsertDebugUtilsLabelEXT_ptr;
	extern PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT_ptr;
	extern PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT_ptr;
	extern PFN_vkQueueBeginDebugUtilsLabelEXT vkQueueBeginDebugUtilsLabelEXT_ptr;
	extern PFN_vkQueueEndDebugUtilsLabelEXT vkQueueEndDebugUtilsLabelEXT_ptr;
	extern PFN_vkQueueInsertDebugUtilsLabelEXT vkQueueInsertDebugUtilsLabelEXT_ptr;
	extern PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectNameEXT_ptr;
	extern PFN_vkSetDebugUtilsObjectTagEXT vkSetDebugUtilsObjectTagEXT_ptr;
	extern PFN_vkSubmitDebugUtilsMessageEXT vkSubmitDebugUtilsMessageEXT_ptr;

	void StartLabel( VkCommandBuffer cmd, const std::string& name, vec4 color);
	void EndLabel( VkCommandBuffer cmd );
}

struct ImmediateExecutor {
	VkFence fence;
	VkCommandBuffer command_buffer;
	VkCommandPool pool;

	std::mutex mutex;

	enum class Error {

	};

	template<typename T = void>
	using Result = std::expected<T, Error>;

	Result<> init( GfxDevice* gfx );
	void execute( std::function<void( VkCommandBuffer )>&& func );
	void cleanup( );

private:
	GfxDevice* gfx;
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
	GpuBuffer allocate( size_t size, VkBufferUsageFlags usage, VmaMemoryUsage vma_usage, const std::string& name );
	void free( const GpuBuffer& buffer );
	void cleanup( );

	VkDescriptorSetLayout getBindlessLayout( ) const;
	VkDescriptorSet getBindlessSet( ) const;

	VkInstance instance;
	VkPhysicalDevice chosen_gpu;
	VkDevice device;

	VkDebugUtilsMessengerEXT debug_messenger;
	std::unordered_map<std::string, uint64_t> allocation_counter;

	VkQueue graphics_queue;
	uint32_t graphics_queue_family;

	ImmediateExecutor executor;
	VmaAllocator allocator;

	VkSurfaceKHR surface;
	::Swapchain swapchain;

	ImageCodex image_codex;
	MaterialCodex material_codex;
	MeshCodex mesh_codex;

private:
	Result<> initDevice( struct SDL_Window* window );
	Result<> initAllocator( );
	void initDebugFunctions( ) const;

	DeletionQueue deletion_queue;
};