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

#include <pch.h>
#include <vk_initializers.h>

VkCommandPoolCreateInfo vk_init::CommandPoolCreateInfo( const uint32_t queueFamilyIndex, const VkCommandPoolCreateFlags flags ) {
    const VkCommandPoolCreateInfo info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = flags,
            .queueFamilyIndex = queueFamilyIndex,
    };

    return info;
}

VkCommandBufferAllocateInfo vk_init::CommandBufferAllocateInfo( const VkCommandPool pool, const uint32_t count ) {
    const VkCommandBufferAllocateInfo info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = nullptr,
            .commandPool = pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = count,
    };
    return info;
}

VkCommandBufferBeginInfo vk_init::CommandBufferBeginInfo( const VkCommandBufferUsageFlags flags ) {
    const VkCommandBufferBeginInfo info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = nullptr,
            .flags = flags,
            .pInheritanceInfo = nullptr,
    };
    return info;
}

VkFenceCreateInfo vk_init::FenceCreateInfo( const VkFenceCreateFlags flags ) {
    const VkFenceCreateInfo info = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext = nullptr,
            .flags = flags,
    };
    return info;
}

VkSemaphoreCreateInfo vk_init::SemaphoreCreateInfo( const VkSemaphoreCreateFlags flags ) {
    const VkSemaphoreCreateInfo info = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = nullptr,
            .flags = flags,
    };
    return info;
}

VkSemaphoreSubmitInfo vk_init::SemaphoreSubmitInfo( const VkPipelineStageFlags2 stageMask, const VkSemaphore semaphore ) {
    const VkSemaphoreSubmitInfo info = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .pNext = nullptr,
            .semaphore = semaphore,
            .value = 1,
            .stageMask = stageMask,
            .deviceIndex = 0,
    };
    return info;
}

VkCommandBufferSubmitInfo vk_init::CommandBufferSubmitInfo( const VkCommandBuffer cmd ) {
    const VkCommandBufferSubmitInfo info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
            .pNext = nullptr,
            .commandBuffer = cmd,
            .deviceMask = 0,
    };
    return info;
}

VkSubmitInfo2 vk_init::SubmitInfo( VkCommandBufferSubmitInfo *cmd, VkSemaphoreSubmitInfo *signalSemaphoreInfo, VkSemaphoreSubmitInfo *waitSemaphoreInfo ) {
    const VkSubmitInfo2 info = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
            .pNext = nullptr,
            .flags = 0,
            .waitSemaphoreInfoCount = waitSemaphoreInfo == nullptr ? 0u : 1u,
            .pWaitSemaphoreInfos = waitSemaphoreInfo,
            .commandBufferInfoCount = 1,
            .pCommandBufferInfos = cmd,
            .signalSemaphoreInfoCount = signalSemaphoreInfo == nullptr ? 0u : 1u,
            .pSignalSemaphoreInfos = signalSemaphoreInfo,
    };
    return info;
}

VkPresentInfoKHR vk_init::PresentInfo( ) {
    const VkPresentInfoKHR info = {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .pNext = nullptr,
            .waitSemaphoreCount = 0,
            .pWaitSemaphores = nullptr,
            .swapchainCount = 0,
            .pSwapchains = nullptr,
            .pImageIndices = nullptr,
            .pResults = nullptr,
    };
    return info;
}

VkRenderingAttachmentInfo vk_init::AttachmentInfo( const VkImageView view, const VkClearValue *clear, const VkImageLayout layout ) {
    const VkRenderingAttachmentInfo color_attachment = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .pNext = nullptr,
            .imageView = view,
            .imageLayout = layout,
            .loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue = clear ? *clear : VkClearValue{ },
    };
    return color_attachment;
}

VkRenderingAttachmentInfo vk_init::DepthAttachmentInfo( const VkImageView view, const VkImageLayout layout ) {
    const VkRenderingAttachmentInfo depth_attachment = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .pNext = nullptr,
            .imageView = view,
            .imageLayout = layout,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue = { .depthStencil = { .depth = 1.f } },
    };
    return depth_attachment;
}

VkRenderingInfo vk_init::RenderingInfo( const VkExtent2D renderExtent, VkRenderingAttachmentInfo *colorAttachment, VkRenderingAttachmentInfo *depthAttachment ) {
    const VkRenderingInfo render_info = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .pNext = nullptr,
            .flags = 0,
            .renderArea = { .offset = { 0, 0 }, .extent = renderExtent },
            .layerCount = 1,
            .viewMask = 0,
            .colorAttachmentCount = 1,
            .pColorAttachments = colorAttachment,
            .pDepthAttachment = depthAttachment,
            .pStencilAttachment = nullptr,
    };
    return render_info;
}

VkImageSubresourceRange vk_init::ImageSubresourceRange( const VkImageAspectFlags aspectMask ) {
    const VkImageSubresourceRange sub_image = {
            .aspectMask = aspectMask,
            .baseMipLevel = 0,
            .levelCount = VK_REMAINING_MIP_LEVELS,
            .baseArrayLayer = 0,
            .layerCount = VK_REMAINING_ARRAY_LAYERS,
    };
    return sub_image;
}

VkDescriptorSetLayoutBinding vk_init::DescriptorSetLayoutBinding( const VkDescriptorType type, const VkShaderStageFlags stageFlags, const uint32_t binding ) {
    const VkDescriptorSetLayoutBinding set_bind = {
            .binding = binding,
            .descriptorType = type,
            .descriptorCount = 1,
            .stageFlags = stageFlags,
            .pImmutableSamplers = nullptr,
    };
    return set_bind;
}

VkDescriptorSetLayoutCreateInfo vk_init::DescriptorSetLayoutCreateInfo( VkDescriptorSetLayoutBinding *bindings, const uint32_t bindingCount ) {
    const VkDescriptorSetLayoutCreateInfo info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .bindingCount = bindingCount,
            .pBindings = bindings,
    };
    return info;
}

VkWriteDescriptorSet vk_init::WriteDescriptorImage( VkDescriptorType type, const VkDescriptorSet dstSet, VkDescriptorImageInfo *imageInfo, const uint32_t binding ) {
    const VkWriteDescriptorSet write = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = dstSet,
            .dstBinding = binding,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = type,
            .pImageInfo = imageInfo,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr,
    };
    return write;
}

VkWriteDescriptorSet vk_init::WriteDescriptorBuffer( VkDescriptorType type, const VkDescriptorSet dstSet, VkDescriptorBufferInfo *bufferInfo, const uint32_t binding ) {
    const VkWriteDescriptorSet write = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = dstSet,
            .dstBinding = binding,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = type,
            .pImageInfo = nullptr,
            .pBufferInfo = bufferInfo,
            .pTexelBufferView = nullptr,
    };
    return write;
}

VkDescriptorBufferInfo vk_init::BufferInfo( const VkBuffer buffer, const VkDeviceSize offset, const VkDeviceSize range ) {
    const VkDescriptorBufferInfo info = {
            .buffer = buffer,
            .offset = offset,
            .range = range,
    };
    return info;
}

VkImageCreateInfo vk_init::ImageCreateInfo( const VkFormat format, const VkImageUsageFlags usageFlags, const VkExtent3D extent ) {
    const VkImageCreateInfo info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = format,
            .extent = extent,
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = usageFlags,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    return info;
}

VkImageViewCreateInfo vk_init::ImageviewCreateInfo( const VkFormat format, const VkImage image, const VkImageAspectFlags aspectFlags ) {
    const VkImageViewCreateInfo info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .image = image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = format,
            .components = {
                    .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .a = VK_COMPONENT_SWIZZLE_IDENTITY,
            },
            .subresourceRange = {
                    .aspectMask = aspectFlags,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
            },
    };
    return info;
}

VkPipelineLayoutCreateInfo vk_init::PipelineLayoutCreateInfo( ) {
    const VkPipelineLayoutCreateInfo info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .setLayoutCount = 0,
            .pSetLayouts = nullptr,
            .pushConstantRangeCount = 0,
            .pPushConstantRanges = nullptr,
    };
    return info;
}

VkPipelineShaderStageCreateInfo vk_init::PipelineShaderStageCreateInfo( const VkShaderStageFlagBits stage, const VkShaderModule shaderModule, const char *entry ) {
    const VkPipelineShaderStageCreateInfo info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = stage,
            .module = shaderModule,
            .pName = entry,
            .pSpecializationInfo = nullptr,
    };
    return info;
}
