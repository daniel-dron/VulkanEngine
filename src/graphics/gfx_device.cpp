#include "gfx_device.h"

#include <VkBootstrap.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <vk_initializers.h>

using namespace vkb;

const bool B_USE_VALIDATION_LAYERS = true;

ImmediateExecutor::Result<> ImmediateExecutor::init( GfxDevice* gfx ) {
	this->gfx = gfx;

	// ----------
	// Fence creation
	const VkFenceCreateInfo fence_info = {
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.pNext = nullptr,
		.flags = VK_FENCE_CREATE_SIGNALED_BIT
	};
	VK_CHECK( vkCreateFence( gfx->device, &fence_info, nullptr, &fence ) );

	// ----------
	// Pool and buffer creation
	const VkCommandPoolCreateInfo pool_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.pNext = nullptr,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = gfx->graphics_queue_family
	};
	VK_CHECK( vkCreateCommandPool( gfx->device, &pool_info, nullptr, &pool ) );

	const VkCommandBufferAllocateInfo buffer_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.pNext = nullptr,
		.commandPool = pool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1
	};
	VK_CHECK( vkAllocateCommandBuffers( gfx->device, &buffer_info, &command_buffer ) );

	return {};
}

void ImmediateExecutor::execute( std::function<void( VkCommandBuffer cmd )>&& func ) {
	VK_CHECK( vkResetFences( gfx->device, 1, &fence ) );
	VK_CHECK( vkResetCommandBuffer( command_buffer, 0 ) );

	auto cmd = command_buffer;
	const VkCommandBufferBeginInfo info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.pNext = nullptr,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		.pInheritanceInfo = nullptr
	};
	VK_CHECK( vkBeginCommandBuffer( cmd, &info ) );

	func( cmd );

	VK_CHECK( vkEndCommandBuffer( cmd ) );

	const VkCommandBufferSubmitInfo submit_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
		.pNext = nullptr,
		.commandBuffer = cmd,
		.deviceMask = 0
	};
	const VkSubmitInfo2 submit = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
		.pNext = nullptr,
		.waitSemaphoreInfoCount = 0,
		.pWaitSemaphoreInfos = nullptr,
		.commandBufferInfoCount = 1,
		.pCommandBufferInfos = &submit_info,
		.signalSemaphoreInfoCount = 0,
		.pSignalSemaphoreInfos = nullptr,
	};

	VK_CHECK( vkQueueSubmit2( gfx->graphics_queue, 1, &submit, fence ) );
	VK_CHECK( vkWaitForFences( gfx->device, 1, &fence, true, 9999999999 ) );
}

void ImmediateExecutor::cleanup( ) {
	vkDestroyFence( gfx->device, fence, nullptr );
	vkDestroyCommandPool( gfx->device, pool, nullptr );
}

GfxDevice::Result<> GfxDevice::init( SDL_Window* window ) {
	RETURN_IF_ERROR( initDevice( window ) );
	RETURN_IF_ERROR( initAllocator( ) );

	initDebugFunctions( );

	executor.init( this );

	image_codex.init( this );

	swapchain.init( this, 1920, 1080 );

	return {};
}

void GfxDevice::execute( std::function<void( VkCommandBuffer )>&& func ) {
	executor.execute( std::move( func ) );
}

AllocatedBuffer GfxDevice::allocate( size_t size, VkBufferUsageFlags usage, VmaMemoryUsage vma_usage, const std::string& name ) {
	const VkBufferCreateInfo info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.pNext = nullptr,
		.size = size,
		.usage = usage
	};

	const VmaAllocationCreateInfo vma_info = {
		.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT,
		.usage = vma_usage
	};

	AllocatedBuffer buffer{};
	VK_CHECK( vmaCreateBuffer( allocator, &info, &vma_info, &buffer.buffer, &buffer.allocation, &buffer.info ) );
	buffer.name = name;

#ifdef ENABLE_DEBUG_UTILS
	allocation_counter[name]++;
	const VkDebugUtilsObjectNameInfoEXT obj = {
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
		.pNext = nullptr,
		.objectType = VkObjectType::VK_OBJECT_TYPE_BUFFER,
		.objectHandle = (uint64_t)buffer.buffer,
		.pObjectName = name.c_str( )
	};
	vkSetDebugUtilsObjectNameEXT( device, &obj );
#endif

	return buffer;
}

void GfxDevice::free( const AllocatedBuffer& buffer ) {

#ifdef ENABLE_DEBUG_UTILS
	allocation_counter[buffer.name]--;
#endif

	vmaDestroyBuffer( allocator, buffer.buffer, buffer.allocation );
}

void GfxDevice::cleanup( ) {
	swapchain.cleanup( );
	image_codex.cleanup( );
	executor.cleanup( );
	vmaDestroyAllocator( allocator );
	vkDestroySurfaceKHR( instance, surface, nullptr );
	vkDestroyDevice( device, nullptr );
	vkb::destroy_debug_utils_messenger( instance, debug_messenger );
	vkDestroyInstance( instance, nullptr );
}

GfxDevice::Result<> GfxDevice::initDevice( SDL_Window* window ) {
	// ----------
	// Instance
	InstanceBuilder builder;
	auto instance_prototype = builder.set_app_name( "Vulkan Engine" )
		.request_validation_layers( B_USE_VALIDATION_LAYERS )
		.enable_extension( "VK_EXT_debug_utils" )
		.use_default_debug_messenger( )
		.require_api_version( 1, 3, 0 )
		.build( );

	if ( !instance_prototype.has_value( ) ) {
		return std::unexpected( GfxDeviceError{ Error::InstanceCreationFailed } );
	}

	auto& bs_instance = instance_prototype.value( );

	instance = bs_instance.instance;
	debug_messenger = bs_instance.debug_messenger;

	// Surface
	SDL_Vulkan_CreateSurface( window, instance, &surface );

	// ----------
	// Device
	VkPhysicalDeviceVulkan13Features features_13{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
		.synchronization2 = true,
		.dynamicRendering = true
	};
	VkPhysicalDeviceVulkan12Features features_12{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
		.descriptorIndexing = true,
		.bufferDeviceAddress = true
	};
	VkPhysicalDeviceFeatures features{
		.fillModeNonSolid = true
	};

	PhysicalDeviceSelector selector{ bs_instance };
	auto physical_device_res = selector
		.set_minimum_version( 1, 3 )
		.set_required_features_13( features_13 )
		.set_required_features_12( features_12 )
		.set_required_features( features )
		.set_surface( surface )
		.select( );
	if ( !physical_device_res ) {
		return std::unexpected( GfxDeviceError{ Error::PhysicalDeviceSelectionFailed } );
	}

	auto& physical_device = physical_device_res.value( );
	chosen_gpu = physical_device;

	// ----------
	// Logical Device
	DeviceBuilder device_builder{ physical_device };
	auto device_res = device_builder.build( );
	if ( !device_res ) {
		return std::unexpected( GfxDeviceError{ Error::LogicalDeviceCreationFailed } );
	}

	auto& bs_device = device_res.value( );
	device = bs_device;

	// ---------
	// Queue
	graphics_queue = bs_device.get_queue( QueueType::graphics ).value( );
	graphics_queue_family = bs_device.get_queue_index( QueueType::graphics ).value( );

	return {};
}

GfxDevice::Result<> GfxDevice::initAllocator( ) {
	VmaAllocatorCreateInfo info = {
		.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
		.physicalDevice = chosen_gpu,
		.device = device,
		.instance = instance,
	};

	if ( vmaCreateAllocator( &info, &allocator ) != VK_SUCCESS ) {
		return std::unexpected( GfxDeviceError{ Error::GlobalAllocatorFailed } );
	}

	return {};
}

void GfxDevice::initDebugFunctions( ) const {
	// ----------
	// debug utils functions
	Debug::vkCmdBeginDebugUtilsLabelEXT_ptr = (PFN_vkCmdBeginDebugUtilsLabelEXT)vkGetInstanceProcAddr( instance, "vkCmdBeginDebugUtilsLabelEXT" );
	Debug::vkCmdEndDebugUtilsLabelEXT_ptr = (PFN_vkCmdEndDebugUtilsLabelEXT)vkGetInstanceProcAddr( instance, "vkCmdEndDebugUtilsLabelEXT" );
	Debug::vkCmdInsertDebugUtilsLabelEXT_ptr = (PFN_vkCmdInsertDebugUtilsLabelEXT)vkGetInstanceProcAddr( instance, "vkCmdInsertDebugUtilsLabelEXT" );
	Debug::vkCreateDebugUtilsMessengerEXT_ptr = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr( instance, "vkCreateDebugUtilsMessengerEXT" );
	Debug::vkDestroyDebugUtilsMessengerEXT_ptr = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr( instance, "vkDestroyDebugUtilsMessengerEXT" );
	Debug::vkQueueBeginDebugUtilsLabelEXT_ptr = (PFN_vkQueueBeginDebugUtilsLabelEXT)vkGetInstanceProcAddr( instance, "vkQueueBeginDebugUtilsLabelEXT" );
	Debug::vkQueueEndDebugUtilsLabelEXT_ptr = (PFN_vkQueueEndDebugUtilsLabelEXT)vkGetInstanceProcAddr( instance, "vkQueueEndDebugUtilsLabelEXT" );
	Debug::vkQueueInsertDebugUtilsLabelEXT_ptr = (PFN_vkQueueInsertDebugUtilsLabelEXT)vkGetInstanceProcAddr( instance, "vkQueueInsertDebugUtilsLabelEXT" );
	Debug::vkSetDebugUtilsObjectNameEXT_ptr = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetInstanceProcAddr( instance, "vkSetDebugUtilsObjectNameEXT" );
	Debug::vkSetDebugUtilsObjectTagEXT_ptr = (PFN_vkSetDebugUtilsObjectTagEXT)vkGetInstanceProcAddr( instance, "vkSetDebugUtilsObjectTagEXT" );
	Debug::vkSubmitDebugUtilsMessageEXT_ptr = (PFN_vkSubmitDebugUtilsMessageEXT)vkGetInstanceProcAddr( instance, "vkSubmitDebugUtilsMessageEXT" );
}

namespace Debug {
	PFN_vkCmdBeginDebugUtilsLabelEXT vkCmdBeginDebugUtilsLabelEXT_ptr = nullptr;
	PFN_vkCmdEndDebugUtilsLabelEXT vkCmdEndDebugUtilsLabelEXT_ptr = nullptr;
	PFN_vkCmdInsertDebugUtilsLabelEXT vkCmdInsertDebugUtilsLabelEXT_ptr = nullptr;
	PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT_ptr = nullptr;
	PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT_ptr = nullptr;
	PFN_vkQueueBeginDebugUtilsLabelEXT vkQueueBeginDebugUtilsLabelEXT_ptr = nullptr;
	PFN_vkQueueEndDebugUtilsLabelEXT vkQueueEndDebugUtilsLabelEXT_ptr = nullptr;
	PFN_vkQueueInsertDebugUtilsLabelEXT vkQueueInsertDebugUtilsLabelEXT_ptr = nullptr;
	PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectNameEXT_ptr = nullptr;
	PFN_vkSetDebugUtilsObjectTagEXT vkSetDebugUtilsObjectTagEXT_ptr = nullptr;
	PFN_vkSubmitDebugUtilsMessageEXT vkSubmitDebugUtilsMessageEXT_ptr = nullptr;

	void StartLabel( VkCommandBuffer cmd, const std::string& name, vec4 color ) {
		const VkDebugUtilsLabelEXT label = {
			.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
			.pNext = nullptr,
			.pLabelName = name.c_str( ),
			.color = { color[0], color[1], color[2], color[3] }
		};
		vkCmdBeginDebugUtilsLabelEXT( cmd, &label );
	}
	void EndLabel( VkCommandBuffer cmd ) {
		vkCmdEndDebugUtilsLabelEXT( cmd );
	}
}

// definitions for linkage to the vulkan library
void vkCmdBeginDebugUtilsLabelEXT( VkCommandBuffer commandBuffer, const VkDebugUtilsLabelEXT* pLabelInfo ) {
	return Debug::vkCmdBeginDebugUtilsLabelEXT_ptr( commandBuffer, pLabelInfo );
}

void vkCmdEndDebugUtilsLabelEXT( VkCommandBuffer commandBuffer ) {
	return Debug::vkCmdEndDebugUtilsLabelEXT_ptr( commandBuffer );
}

void vkCmdInsertDebugUtilsLabelEXT( VkCommandBuffer commandBuffer, const VkDebugUtilsLabelEXT* pLabelInfo ) {
	return Debug::vkCmdInsertDebugUtilsLabelEXT_ptr( commandBuffer, pLabelInfo );
}

VkResult vkCreateDebugUtilsMessengerEXT( VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pMessenger ) {
	return Debug::vkCreateDebugUtilsMessengerEXT_ptr( instance, pCreateInfo, pAllocator, pMessenger );
}

void vkDestroyDebugUtilsMessengerEXT( VkInstance instance, VkDebugUtilsMessengerEXT messenger, const VkAllocationCallbacks* pAllocator ) {
	return Debug::vkDestroyDebugUtilsMessengerEXT_ptr( instance, messenger, pAllocator );
}

void vkQueueBeginDebugUtilsLabelEXT( VkQueue queue, const VkDebugUtilsLabelEXT* pLabelInfo ) {
	return Debug::vkQueueBeginDebugUtilsLabelEXT_ptr( queue, pLabelInfo );
}

void vkQueueEndDebugUtilsLabelEXT( VkQueue queue ) {
	return Debug::vkQueueEndDebugUtilsLabelEXT_ptr( queue );
}

void vkQueueInsertDebugUtilsLabelEXT( VkQueue queue, const VkDebugUtilsLabelEXT* pLabelInfo ) {
	return Debug::vkQueueInsertDebugUtilsLabelEXT_ptr( queue, pLabelInfo );
}

VkResult vkSetDebugUtilsObjectNameEXT( VkDevice device, const VkDebugUtilsObjectNameInfoEXT* pNameInfo ) {
	return Debug::vkSetDebugUtilsObjectNameEXT_ptr( device, pNameInfo );
}

VkResult vkSetDebugUtilsObjectTagEXT( VkDevice device, const VkDebugUtilsObjectTagInfoEXT* pTagInfo ) {
	return Debug::vkSetDebugUtilsObjectTagEXT_ptr( device, pTagInfo );
}

void vkSubmitDebugUtilsMessageEXT( VkInstance instance, VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData ) {
	return Debug::vkSubmitDebugUtilsMessageEXT_ptr( instance, messageSeverity, messageTypes, pCallbackData );
}

