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

#include "swapchain.h"

#include <vk_initializers.h>
#include "VkBootstrap.h"
#include "gfx_device.h"
#include "imgui_impl_vulkan.h"
#include "vk_types.h"

Swapchain::FrameData &Swapchain::GetCurrentFrame( ) {
    return frames[frameNumber % FrameOverlap];
}

Swapchain::Result<> Swapchain::Init( GfxDevice *gfx, const uint32_t width, const uint32_t height ) {
    this->m_gfx = gfx;
    extent = VkExtent2D{ width, height };

    RETURN_IF_ERROR( Create( width, height ) );

    const VkCommandPoolCreateInfo command_pool_info = vk_init::CommandPoolCreateInfo( gfx->graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT );
    for ( int i = 0; i < FrameOverlap; i++ ) {
        VK_CHECK( vkCreateCommandPool( gfx->device, &command_pool_info, nullptr, &frames[i].pool ) );

        VkCommandBufferAllocateInfo cmd_alloc_info = vk_init::CommandBufferAllocateInfo( frames[i].pool, 1 );
        VK_CHECK( vkAllocateCommandBuffers( gfx->device, &cmd_alloc_info, &frames[i].commandBuffer ) );
    }

    auto fenceCreateInfo = vk_init::FenceCreateInfo( VK_FENCE_CREATE_SIGNALED_BIT );
    auto semaphoreCreateInfo = vk_init::SemaphoreCreateInfo( );

    for ( int i = 0; i < FrameOverlap; i++ ) {
        VK_CHECK( vkCreateFence( gfx->device, &fenceCreateInfo, nullptr, &frames[i].fence ) );

        VK_CHECK( vkCreateSemaphore( gfx->device, &semaphoreCreateInfo, nullptr, &frames[i].renderSemaphore ) );
        VK_CHECK( vkCreateSemaphore( gfx->device, &semaphoreCreateInfo, nullptr, &frames[i].swapchainSemaphore ) );
    }

    return { };
}

void Swapchain::Cleanup( ) {
    for ( uint64_t i = 0; i < FrameOverlap; i++ ) {
        vkDestroyCommandPool( m_gfx->device, frames[i].pool, nullptr );

        // sync objects
        vkDestroyFence( m_gfx->device, frames[i].fence, nullptr );
        vkDestroySemaphore( m_gfx->device, frames[i].renderSemaphore, nullptr );
        vkDestroySemaphore( m_gfx->device, frames[i].swapchainSemaphore, nullptr );

        frames[i].deletionQueue.Flush( );
    }

    vkDestroySwapchainKHR( m_gfx->device, swapchain, nullptr );

    for ( const auto &view : views ) {
        vkDestroyImageView( m_gfx->device, view, nullptr );
    }

    vkDestroySampler( m_gfx->device, m_linear, nullptr );
}

Swapchain::Result<> Swapchain::Recreate( const uint32_t width, const uint32_t height ) {
    const auto old = swapchain;
    const auto old_views = views;

    vkb::SwapchainBuilder builder{ m_gfx->chosenGpu, m_gfx->device, m_gfx->surface };
    format = VK_FORMAT_R8G8B8A8_SRGB;

    auto swapchain_res = builder
                                 .set_desired_format( VkSurfaceFormatKHR{
                                         .format = format,
                                         .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
                                 } )
                                 .set_desired_present_mode( presentMode )
                                 .add_image_usage_flags( VK_IMAGE_USAGE_TRANSFER_DST_BIT )
                                 .set_old_swapchain( swapchain )
                                 .build( );

    if ( !swapchain_res ) {
        return std::unexpected( Error{ } );
    }

    auto &bs_swapchain = swapchain_res.value( );

    swapchain = bs_swapchain.swapchain;
    images = bs_swapchain.get_images( ).value( );
    views = bs_swapchain.get_image_views( ).value( );

    extent = VkExtent2D{ width, height };

    // Recreate and destroy left over primitives
    vkDestroySemaphore( m_gfx->device, GetCurrentFrame( ).swapchainSemaphore, nullptr );
    constexpr VkSemaphoreCreateInfo info = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = nullptr,
    };
    VK_CHECK( vkCreateSemaphore( m_gfx->device, &info, nullptr, &GetCurrentFrame( ).swapchainSemaphore ) );

    vkDestroySwapchainKHR( m_gfx->device, old, nullptr );
    for ( const auto &view : old_views ) {
        vkDestroyImageView( m_gfx->device, view, nullptr );
    }

    CreateFrameImages( );

    return { };
}

Swapchain::Result<> Swapchain::Create( uint32_t width, uint32_t height ) {
    vkb::SwapchainBuilder builder{ m_gfx->chosenGpu, m_gfx->device, m_gfx->surface };
    format = VK_FORMAT_R8G8B8A8_SRGB;

    auto swapchain_res = builder
                                 .set_desired_format( VkSurfaceFormatKHR{
                                         .format = format,
                                         .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
                                 } )
                                 .set_desired_present_mode( presentMode )
                                 .add_image_usage_flags( VK_IMAGE_USAGE_TRANSFER_DST_BIT )
                                 .build( );

    if ( !swapchain_res ) {
        return std::unexpected( Error{ } );
    }

    auto &bs_swapchain = swapchain_res.value( );

    swapchain = bs_swapchain.swapchain;
    images = bs_swapchain.get_images( ).value( );
    views = bs_swapchain.get_image_views( ).value( );

    CreateFrameImages( );
    CreateGBuffers( );

    return { };
}

void Swapchain::CreateFrameImages( ) {
    const VkExtent3D draw_image_extent = {
            .width = extent.width, .height = extent.height, .depth = 1 };

    VkImageUsageFlags draw_image_usages{ };
    draw_image_usages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    draw_image_usages |= VK_IMAGE_USAGE_SAMPLED_BIT;

    VkImageUsageFlags depth_image_usages{ };
    depth_image_usages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    depth_image_usages |= VK_IMAGE_USAGE_SAMPLED_BIT;

    // TODO: transition to correct layout ( check validation layers )
    for ( auto &frame : frames ) {
        std::vector<unsigned char> empty_image_data;
        empty_image_data.resize( extent.width * extent.height * 8, 0 );
        frame.hdrColor = m_gfx->imageCodex.LoadImageFromData( "hdr image pbr", empty_image_data.data( ), draw_image_extent, VK_FORMAT_R16G16B16A16_SFLOAT, draw_image_usages, false );

        frame.ssao = m_gfx->imageCodex.CreateEmptyImage( "SSAO", draw_image_extent, VK_FORMAT_R32G32B32A32_SFLOAT, draw_image_usages | VK_IMAGE_USAGE_STORAGE_BIT, false );

        frame.postProcessImage = m_gfx->imageCodex.CreateEmptyImage( "post process", draw_image_extent, VK_FORMAT_R8G8B8A8_SRGB, draw_image_usages | VK_IMAGE_USAGE_STORAGE_BIT, false );

        empty_image_data.resize( extent.width * extent.height * 4, 0 );
        frame.depth = m_gfx->imageCodex.LoadImageFromData( "main depth image", empty_image_data.data( ), draw_image_extent, VK_FORMAT_D32_SFLOAT, depth_image_usages, false );
    }
}

void Swapchain::CreateGBuffers( ) {
    constexpr VkImageUsageFlags usages = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    const VkExtent3D extent = { .width = this->extent.width, .height = this->extent.height, .depth = 1 };

    for ( auto &frame : frames ) {
        std::vector<unsigned char> empty_data;
        empty_data.resize( extent.width * extent.height * 4 * 2, 0 );

        frame.gBuffer.position = m_gfx->imageCodex.LoadImageFromData( "gbuffer.position", empty_data.data( ), extent, VK_FORMAT_R16G16B16A16_SFLOAT, usages, false );
        frame.gBuffer.normal = m_gfx->imageCodex.LoadImageFromData( "gbuffer.normal", empty_data.data( ), extent, VK_FORMAT_R16G16B16A16_SFLOAT, usages, false );
        frame.gBuffer.pbr = m_gfx->imageCodex.LoadImageFromData( "gbuffer.pbr", empty_data.data( ), extent, VK_FORMAT_R16G16B16A16_SFLOAT, usages, false );
        frame.gBuffer.albedo = m_gfx->imageCodex.LoadImageFromData( "gbuffer.albedo", empty_data.data( ), extent, VK_FORMAT_R16G16B16A16_SFLOAT, usages, false );
    }
}
