#include "gfx_device.h"

#include <VkBootstrap.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <vk_initializers.h>

using namespace vkb;

const bool B_USE_VALIDATION_LAYERS = true;

ImmediateExecutor::Result<> ImmediateExecutor::init( GfxDevice* gfx ) {

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

void ImmediateExecutor::execute( GfxDevice* gfx, std::function<void( VkCommandBuffer cmd )>&& func ) {
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

void ImmediateExecutor::cleanup( GfxDevice* gfx ) {
	vkDestroyFence( gfx->device, fence, nullptr );
	vkDestroyCommandPool( gfx->device, pool, nullptr );
}

GfxDevice::Result<> GfxDevice::init( SDL_Window* window ) {
	RETURN_IF_ERROR( initDevice( window ) );
	RETURN_IF_ERROR( initAllocator( ) );

	executor.init( this );

	swapchain.init( this, 2560, 1440 );
}

void GfxDevice::execute( std::function<void( VkCommandBuffer )>&& func ) {
	executor.execute( this, std::move( func ) );
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

	return buffer;
}

void GfxDevice::free( const AllocatedBuffer& buffer ) {
	vmaDestroyBuffer( allocator, buffer.buffer, buffer.allocation );
}

void GfxDevice::cleanup( ) {
	swapchain.cleanup( this );
	image_codex.cleanup( );
	executor.cleanup( this );
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
