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

#include <expected>
#include <mutex>
#include <vk_types.h>

#include "descriptors.h"
#include "image_codex.h"
#include "material_codex.h"
#include "mesh_codex.h"
#include "swapchain.h"

#include "shader_storage.h"

class TL_VkContext;
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

	void StartLabel( VkCommandBuffer cmd, const std::string& name, Vec4 color);
	void EndLabel( VkCommandBuffer cmd );
}

struct ImmediateExecutor {
	VkFence fence;
	VkCommandBuffer commandBuffer;
	VkCommandPool pool;

	std::mutex mutex;

	enum class Error {

	};

	template<typename T = void>
	using Result = std::expected<T, Error>;

	Result<> Init( TL_VkContext* gfx );
	void Execute( std::function<void( VkCommandBuffer )>&& func );
	void Cleanup( ) const;

private:
	TL_VkContext *m_gfx = nullptr;
};

class TL_VkContext {
public:
	using Ptr = TL_VkContext*;
	using Ref = std::shared_ptr<TL_VkContext>;

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

	Result<> Init( struct SDL_Window* window );
	void Execute( std::function<void( VkCommandBuffer )>&& func );
	GpuBuffer Allocate( size_t size, VkBufferUsageFlags usage, VmaMemoryUsage vmaUsage, const std::string& name );
	void Free( const GpuBuffer& buffer );
	void Cleanup( );

	VkDescriptorSet AllocateSet( VkDescriptorSetLayout layout );
    MultiDescriptorSet AllocateMultiSet( VkDescriptorSetLayout layout );

	VkDescriptorSetLayout GetBindlessLayout( ) const;
	VkDescriptorSet GetBindlessSet( ) const;

    float GetTimestampInMs(uint64_t start, uint64_t end ) const;

	DescriptorAllocatorGrowable setPool;

	VkInstance instance;
	VkPhysicalDevice chosenGpu;
	VkDevice device;

	VkPhysicalDeviceProperties2 deviceProperties;
    VkPhysicalDeviceMemoryProperties memProperties;

	VkDebugUtilsMessengerEXT debugMessenger;
	std::unordered_map<std::string, uint64_t> allocationCounter;

	VkQueue graphicsQueue;
	uint32_t graphicsQueueFamily;

	// TODO: implement a command buffer manager (list of commands to get pulled/pushed)
	VkQueue computeQueue;
	uint32_t computeQueueFamily;
	VkCommandPool computeCommandPool;
	VkCommandBuffer computeCommand;

	ImmediateExecutor executor;
	VmaAllocator allocator;

	VkSurfaceKHR surface;
	::TL_Swapchain swapchain;

	ImageCodex imageCodex;
	MaterialCodex materialCodex;
	MeshCodex meshCodex;

	std::unique_ptr<ShaderStorage> shaderStorage;

    VkQueryPool queryPoolTimestamps = nullptr;
    std::array<uint64_t, 10> gpuTimestamps;

	void DrawDebug( ) const;

private:
	Result<> InitDevice( struct SDL_Window* window );
	Result<> InitAllocator( );
	void InitDebugFunctions( ) const;

	DeletionQueue m_deletionQueue;
};