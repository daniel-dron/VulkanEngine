#pragma once

#include <vk_types.h>
#include <expected>
#include <mutex>
#include "image_codex.h"
#include "material_codex.h"
#include "mesh_codex.h"
#include "swapchain.h"

#include "shader_storage.h"

class GfxDevice;
class ShaderStorage;

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
	using Ptr = GfxDevice*;
	using Ref = std::shared_ptr<GfxDevice>;

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

	VkDescriptorSet AllocateSet( VkDescriptorSetLayout layout );

	VkDescriptorSetLayout getBindlessLayout( ) const;
	VkDescriptorSet getBindlessSet( ) const;

	DescriptorAllocatorGrowable set_pool;

	VkInstance instance;
	VkPhysicalDevice chosen_gpu;
	VkDevice device;

	VkPhysicalDeviceProperties2 device_properties;
	VkPhysicalDeviceMemoryProperties mem_properties;

	VkDebugUtilsMessengerEXT debug_messenger;
	std::unordered_map<std::string, uint64_t> allocation_counter;

	VkQueue graphics_queue;
	uint32_t graphics_queue_family;

	// TODO: implement a command buffer manager (list of commands to get pulled/pushed)
	VkQueue compute_queue;
	uint32_t compute_queue_family;
	VkCommandPool compute_command_pool;
	VkCommandBuffer compute_command;

	ImmediateExecutor executor;
	VmaAllocator allocator;

	VkSurfaceKHR surface;
	::Swapchain swapchain;

	ImageCodex image_codex;
	MaterialCodex material_codex;
	MeshCodex mesh_codex;

	std::unique_ptr<ShaderStorage> shader_storage;

	void DrawDebug( ) const;

private:
	Result<> initDevice( struct SDL_Window* window );
	Result<> initAllocator( );
	void initDebugFunctions( ) const;

	DeletionQueue deletion_queue;
};