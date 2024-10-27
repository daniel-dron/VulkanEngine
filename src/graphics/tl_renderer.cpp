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

#include "tl_renderer.h"

#include <graphics/swapchain.h>
#include <vk_engine.h>
#include <vk_initializers.h>

namespace TL {

    u32 StartFrame( const TL_FrameData &frame ) noexcept {
        u32 swapchain_image_index = -1;

        VKCALL( vkWaitForFences( g_TL->gfx->device, 1, &frame.fence, true, UINT64_MAX ) );

        // Reset fence in case the next KHR image acquire fails or we lose device
        VKCALL( vkResetFences( g_TL->gfx->device, 1, &frame.fence ) );

        VKCALL( vkAcquireNextImageKHR( g_TL->gfx->device, g_TL->gfx->swapchain.swapchain, UINT64_MAX,
                                       frame.swapchainSemaphore, nullptr, &swapchain_image_index ) );

        // Start command recording (for now we use one single command buffer for the entire main passes)
        auto cmd = frame.commandBuffer;
        VKCALL( vkResetCommandBuffer( cmd, 0 ) );
        auto cmd_begin_info = vk_init::CommandBufferBeginInfo( VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT );
        VKCALL( vkBeginCommandBuffer( cmd, &cmd_begin_info ) );


        // Query GPU timers from last frame
        if ( g_TL->gfx->swapchain.frameNumber != 0 ) {
            vkGetQueryPoolResults(
                    g_TL->gfx->device, g_TL->gfx->queryPoolTimestamps, 0, ( u32 )g_TL->gfx->gpuTimestamps.size( ),
                    g_TL->gfx->gpuTimestamps.size( ) * sizeof( uint64_t ), g_TL->gfx->gpuTimestamps.data( ),
                    sizeof( uint64_t ), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT );
        }

        // Reset gpu query timers
        vkCmdResetQueryPool( cmd, g_TL->gfx->queryPoolTimestamps, 0, ( u32 )g_TL->gfx->gpuTimestamps.size( ) );

        auto time = g_TL->gfx->GetTimestampInMs( g_TL->gfx->gpuTimestamps.at( 0 ), g_TL->gfx->gpuTimestamps.at( 1 ) ) /
                1000.0f;
        g_visualProfiler.AddTimer( "ShadowMap", time, utils::VisualProfiler::Gpu );
        
        time = g_TL->gfx->GetTimestampInMs( g_TL->gfx->gpuTimestamps.at( 2 ), g_TL->gfx->gpuTimestamps.at( 3 ) ) /
                1000.0f;
        g_visualProfiler.AddTimer( "GBuffer", time, utils::VisualProfiler::Gpu );
        
        time = g_TL->gfx->GetTimestampInMs( g_TL->gfx->gpuTimestamps.at( 4 ), g_TL->gfx->gpuTimestamps.at( 5 ) ) /
                1000.0f;
        g_visualProfiler.AddTimer( "Lighting", time, utils::VisualProfiler::Gpu );
        
        time = g_TL->gfx->GetTimestampInMs( g_TL->gfx->gpuTimestamps.at( 6 ), g_TL->gfx->gpuTimestamps.at( 7 ) ) /
                1000.0f;
        g_visualProfiler.AddTimer( "Skybox", time, utils::VisualProfiler::Gpu );

        time = g_TL->gfx->GetTimestampInMs( g_TL->gfx->gpuTimestamps.at( 8 ), g_TL->gfx->gpuTimestamps.at( 9 ) ) /
                1000.0f;
        g_visualProfiler.AddTimer( "Post Process", time, utils::VisualProfiler::Gpu );

        return swapchain_image_index;
    }

    void EndFrame( const TL_FrameData &frame, u32 swapchain_image_index ) noexcept {
        VKCALL( vkEndCommandBuffer( frame.commandBuffer ) );

        auto cmd_info = vk_init::CommandBufferSubmitInfo( frame.commandBuffer );
        auto wait_info = vk_init::SemaphoreSubmitInfo( VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
                                                       frame.swapchainSemaphore );
        auto signal_info = vk_init::SemaphoreSubmitInfo( VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, frame.renderSemaphore );
        auto submit = vk_init::SubmitInfo( &cmd_info, &signal_info, &wait_info );
        VKCALL( vkQueueSubmit2( g_TL->gfx->graphicsQueue, 1, &submit, frame.fence ) );
    }

    void Present( const TL_FrameData &frame, u32 swapchain_image_index ) noexcept {
        VkPresentInfoKHR presentInfo = { .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                                         .pNext = nullptr,
                                         // wait on _renderSemaphore, since we need the rendering to have finished
                                         // to display to the screen
                                         .waitSemaphoreCount = 1,
                                         .pWaitSemaphores = &frame.renderSemaphore,

                                         .swapchainCount = 1,
                                         .pSwapchains = &g_TL->gfx->swapchain.swapchain,

                                         .pImageIndices = &swapchain_image_index };

        VKCALL( vkQueuePresentKHR( g_TL->gfx->graphicsQueue, &presentInfo ) );
    }

} // namespace TL