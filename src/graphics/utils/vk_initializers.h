﻿/******************************************************************************
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

// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <graphics/utils/vk_types.h>

namespace vk_init {
	VkCommandPoolCreateInfo CommandPoolCreateInfo(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags = 0 );
	VkCommandBufferAllocateInfo CommandBufferAllocateInfo( VkCommandPool pool, uint32_t count = 1 );

	VkCommandBufferBeginInfo CommandBufferBeginInfo( VkCommandBufferUsageFlags flags = 0 );
	VkCommandBufferSubmitInfo CommandBufferSubmitInfo( VkCommandBuffer cmd );

	VkFenceCreateInfo FenceCreateInfo( VkFenceCreateFlags flags = 0 );

	VkSemaphoreCreateInfo SemaphoreCreateInfo( VkSemaphoreCreateFlags flags = 0 );

	VkSubmitInfo2 SubmitInfo( VkCommandBufferSubmitInfo* cmd, VkSemaphoreSubmitInfo* signalSemaphoreInfo, VkSemaphoreSubmitInfo* waitSemaphoreInfo );
	VkPresentInfoKHR PresentInfo( );

	VkRenderingAttachmentInfo AttachmentInfo( VkImageView view, const VkClearValue * clear, VkImageLayout layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL );

	VkRenderingAttachmentInfo DepthAttachmentInfo( VkImageView view, VkImageLayout layout );

	VkRenderingInfo RenderingInfo( VkExtent2D renderExtent, VkRenderingAttachmentInfo* colorAttachment, VkRenderingAttachmentInfo* depthAttachment );

	VkImageSubresourceRange ImageSubresourceRange( VkImageAspectFlags aspectMask );

	VkSemaphoreSubmitInfo SemaphoreSubmitInfo( VkPipelineStageFlags2 stageMask,
		VkSemaphore semaphore );
	VkDescriptorSetLayoutBinding DescriptorSetLayoutBinding( VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t binding );
	VkDescriptorSetLayoutCreateInfo DescriptorSetLayoutCreateInfo( VkDescriptorSetLayoutBinding* bindings, uint32_t bindingCount );
	VkWriteDescriptorSet WriteDescriptorImage( VkDescriptorType type, VkDescriptorSet dstSet, VkDescriptorImageInfo* imageInfo, uint32_t binding );
	VkWriteDescriptorSet WriteDescriptorBuffer( VkDescriptorType type, VkDescriptorSet dstSet, VkDescriptorBufferInfo* bufferInfo, uint32_t binding );
	VkDescriptorBufferInfo BufferInfo( VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range );

	VkImageCreateInfo ImageCreateInfo( VkFormat format, VkImageUsageFlags usageFlags, VkExtent3D extent );
	VkImageViewCreateInfo ImageviewCreateInfo( VkFormat format, VkImage image, VkImageAspectFlags aspectFlags );
	VkPipelineLayoutCreateInfo PipelineLayoutCreateInfo( );
	VkPipelineShaderStageCreateInfo PipelineShaderStageCreateInfo( VkShaderStageFlagBits stage, VkShaderModule shaderModule, const char* entry = "main" );
}  // namespace vkinit
