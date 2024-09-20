#include "lifetime_tracker.h"

#include <graphics/gfx_device.h>

void LifetimeTracker::init( GfxDevice* gfx ) {
	this->gfx = gfx;
}

void LifetimeTracker::clean( ) {
	for ( const auto& object : objects ) {
		deleteObject( object );
	}

	objects.clear( );
}

LifetimeTracker::~LifetimeTracker( ) {
	clean( );
}

void LifetimeTracker::deleteObject( const VulkanObject& obj ) {
	switch ( obj.type ) {
	case VulkanObjectType::Buffer:
		vkDestroyBuffer( gfx->device, (VkBuffer)obj.handle, nullptr );
		break;
	case VulkanObjectType::Image:
		vkDestroyImage( gfx->device, (VkImage)obj.handle, nullptr );
		break;
	case VulkanObjectType::ImageView:
		vkDestroyImageView( gfx->device, (VkImageView)obj.handle, nullptr );
		break;
	case VulkanObjectType::Sampler:
		vkDestroySampler( gfx->device, (VkSampler)obj.handle, nullptr );
		break;
	case VulkanObjectType::DescriptorSetLayout:
		vkDestroyDescriptorSetLayout( gfx->device, (VkDescriptorSetLayout)obj.handle, nullptr );
		break;
	case VulkanObjectType::DescriptorPool:
		vkDestroyDescriptorPool( gfx->device, (VkDescriptorPool)obj.handle, nullptr );
		break;
	case VulkanObjectType::Pipeline:
		vkDestroyPipeline( gfx->device, (VkPipeline)obj.handle, nullptr );
		break;
	case VulkanObjectType::PipelineLayout:
		vkDestroyPipelineLayout( gfx->device, (VkPipelineLayout)obj.handle, nullptr );
		break;
	case VulkanObjectType::CommandPool:
		vkDestroyCommandPool( gfx->device, (VkCommandPool)obj.handle, nullptr );
		break;
	case VulkanObjectType::Fence:
		vkDestroyFence( gfx->device, (VkFence)obj.handle, nullptr );
		break;
	case VulkanObjectType::Semaphore:
		vkDestroySemaphore( gfx->device, (VkSemaphore)obj.handle, nullptr );
		break;
	case VulkanObjectType::ShaderModule:
		vkDestroyShaderModule( gfx->device, (VkShaderModule)obj.handle, nullptr );
		break;
	default:
		break;
	}
}
