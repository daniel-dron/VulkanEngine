#include "gfx_device.h"

#include <VkBootstrap.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

using namespace vkb;

const bool B_USE_VALIDATION_LAYERS = true;

ImmediateExecutor::Result<> ImmediateExecutor::init( GfxDevice* gfx ) {
	return {};
}

ImmediateExecutor::Result<> ImmediateExecutor::execute( std::function<void( VkCommandBuffer )>&& func ) {
	return {};
}

GfxDevice::Result<> GfxDevice::init( SDL_Window* window ) {
	RETURN_IF_ERROR( initDevice( window ) );
	RETURN_IF_ERROR( initAllocator( ) );

	swapchain.init( this, 2560, 1440 );
}

void GfxDevice::cleanup( ) {
	swapchain.cleanup( this );
	image_codex.cleanup( );
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
