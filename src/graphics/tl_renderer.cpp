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

#include <vk_engine.h>
#include <vk_initializers.h>

namespace TL {

    u32 Renderer::StartFrame( const TL_FrameData &frame ) noexcept {
        u32 swapchain_image_index = -1;

        if ( this->vkctx.frameNumber != 0 ) {
            VKCALL( vkWaitForFences( vkctx.device, 1, &frame.fence, true, UINT64_MAX ) );
        }

        // Reset fence in case the next KHR image acquire fails or we lose device
        VKCALL( vkResetFences( vkctx.device, 1, &frame.fence ) );

        VKCALL( vkAcquireNextImageKHR( vkctx.device, vkctx.swapchain, UINT64_MAX, frame.swapchainSemaphore, nullptr,
                                       &swapchain_image_index ) );

        // Start command recording (for now we use one single command buffer for the entire main passes)
        auto cmd = frame.commandBuffer;
        VKCALL( vkResetCommandBuffer( cmd, 0 ) );
        auto cmd_begin_info = vk_init::CommandBufferBeginInfo( VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT );
        VKCALL( vkBeginCommandBuffer( cmd, &cmd_begin_info ) );


        // Query GPU timers from last frame
        if ( vkctx.frameNumber != 0 ) {
            vkGetQueryPoolResults( vkctx.device, vkctx.queryPoolTimestamps, 0, ( u32 )vkctx.gpuTimestamps.size( ),
                                   vkctx.gpuTimestamps.size( ) * sizeof( uint64_t ), vkctx.gpuTimestamps.data( ),
                                   sizeof( uint64_t ), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT );
        }

        // Reset gpu query timers
        vkCmdResetQueryPool( cmd, vkctx.queryPoolTimestamps, 0, ( u32 )vkctx.gpuTimestamps.size( ) );

        auto time = vkctx.GetTimestampInMs( vkctx.gpuTimestamps.at( 0 ), vkctx.gpuTimestamps.at( 1 ) ) / 1000.0f;
        g_visualProfiler.AddTimer( "ShadowMap", time, utils::VisualProfiler::Gpu );

        time = vkctx.GetTimestampInMs( vkctx.gpuTimestamps.at( 2 ), vkctx.gpuTimestamps.at( 3 ) ) / 1000.0f;
        g_visualProfiler.AddTimer( "GBuffer", time, utils::VisualProfiler::Gpu );

        time = vkctx.GetTimestampInMs( vkctx.gpuTimestamps.at( 4 ), vkctx.gpuTimestamps.at( 5 ) ) / 1000.0f;
        g_visualProfiler.AddTimer( "Lighting", time, utils::VisualProfiler::Gpu );

        time = vkctx.GetTimestampInMs( vkctx.gpuTimestamps.at( 6 ), vkctx.gpuTimestamps.at( 7 ) ) / 1000.0f;
        g_visualProfiler.AddTimer( "Skybox", time, utils::VisualProfiler::Gpu );

        time = vkctx.GetTimestampInMs( vkctx.gpuTimestamps.at( 8 ), vkctx.gpuTimestamps.at( 9 ) ) / 1000.0f;
        g_visualProfiler.AddTimer( "Post Process", time, utils::VisualProfiler::Gpu );

        return swapchain_image_index;
    }

    void Renderer::EndFrame( const TL_FrameData &frame ) const noexcept {
        VKCALL( vkEndCommandBuffer( frame.commandBuffer ) );

        auto cmd_info = vk_init::CommandBufferSubmitInfo( frame.commandBuffer );
        auto wait_info = vk_init::SemaphoreSubmitInfo( VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
                                                       frame.swapchainSemaphore );
        auto signal_info = vk_init::SemaphoreSubmitInfo( VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, frame.renderSemaphore );
        auto submit = vk_init::SubmitInfo( &cmd_info, &signal_info, &wait_info );
        VKCALL( vkQueueSubmit2( vkctx.graphicsQueue, 1, &submit, frame.fence ) );
    }

    void Renderer::Present( const TL_FrameData &frame, u32 swapchain_image_index ) noexcept {
        const VkPresentInfoKHR presentInfo = { .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                                               .pNext = nullptr,
                                               // wait on _renderSemaphore, since we need the rendering to have finished
                                               // to display to the screen
                                               .waitSemaphoreCount = 1,
                                               .pWaitSemaphores = &frame.renderSemaphore,

                                               .swapchainCount = 1,
                                               .pSwapchains = &vkctx.swapchain,

                                               .pImageIndices = &swapchain_image_index };

        VKCALL( vkQueuePresentKHR( vkctx.graphicsQueue, &presentInfo ) );
    }

    Renderer::Renderer( SDL_Window *window ) : vkctx( window ) {}
} // namespace TL