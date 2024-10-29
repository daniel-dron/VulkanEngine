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

    Renderer::Renderer( SDL_Window *window ) : vkctx( window ) {

        // All frames use the same type of gbuffer, so this is fine
        auto &g_buffer = vkctx.GetCurrentFrame( ).gBuffer;

        // TODO: move this
        struct PushConstants {
            Mat4 worldFromLocal;
            VkDeviceAddress sceneDataAddress;
            VkDeviceAddress vertexBufferAddress;
            uint32_t materialId;
        };

        m_gbufferPipeline = std::make_unique<Pipeline>(
                vkctx,
                PipelineConfig{
                        .vertex = "../shaders/gbuffer.vert.spv",
                        .pixel = "../shaders/gbuffer.frag.spv",
                        .colorTargets = { { .format = vkctx.imageCodex.GetImage( g_buffer.albedo ).GetFormat( ),
                                            .blendType = PipelineConfig::BlendType::OFF },
                                          { .format = vkctx.imageCodex.GetImage( g_buffer.normal ).GetFormat( ),
                                            .blendType = PipelineConfig::BlendType::OFF },
                                          { .format = vkctx.imageCodex.GetImage( g_buffer.position ).GetFormat( ),
                                            .blendType = PipelineConfig::BlendType::OFF },
                                          { .format = vkctx.imageCodex.GetImage( g_buffer.pbr ).GetFormat( ),
                                            .blendType = PipelineConfig::BlendType::OFF } },
                        .pushConstantRanges = { { .stageFlags =
                                                          VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                                  .offset = 0,
                                                  .size = sizeof( PushConstants ) } },

                        .descriptorSetLayouts = { vkctx.GetBindlessLayout( ) },
                } );
    }

    void Renderer::StartFrame( ) noexcept {
        const auto &frame = vkctx.GetCurrentFrame( );

        if ( this->vkctx.frameNumber != 0 ) {
            VKCALL( vkWaitForFences( vkctx.device, 1, &frame.fence, true, UINT64_MAX ) );
        }

        // Reset fence in case the next KHR image acquire fails or we lose device
        VKCALL( vkResetFences( vkctx.device, 1, &frame.fence ) );

        VKCALL( vkAcquireNextImageKHR( vkctx.device, vkctx.swapchain, UINT64_MAX, frame.swapchainSemaphore, nullptr,
                                       &swapchainImageIndex ) );

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
    }

    void Renderer::EndFrame( ) noexcept {
        const auto &frame = vkctx.GetCurrentFrame( );

        VKCALL( vkEndCommandBuffer( frame.commandBuffer ) );

        auto cmd_info = vk_init::CommandBufferSubmitInfo( frame.commandBuffer );
        auto wait_info = vk_init::SemaphoreSubmitInfo( VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
                                                       frame.swapchainSemaphore );
        auto signal_info = vk_init::SemaphoreSubmitInfo( VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, frame.renderSemaphore );
        auto submit = vk_init::SubmitInfo( &cmd_info, &signal_info, &wait_info );
        VKCALL( vkQueueSubmit2( vkctx.graphicsQueue, 1, &submit, frame.fence ) );
    }

    void Renderer::Present( ) noexcept {
        const auto &frame = vkctx.GetCurrentFrame( );

        const VkPresentInfoKHR presentInfo = { .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                                               .pNext = nullptr,
                                               // wait on _renderSemaphore, since we need the rendering to have finished
                                               // to display to the screen
                                               .waitSemaphoreCount = 1,
                                               .pWaitSemaphores = &frame.renderSemaphore,

                                               .swapchainCount = 1,
                                               .pSwapchains = &vkctx.swapchain,

                                               .pImageIndices = &swapchainImageIndex };

        VKCALL( vkQueuePresentKHR( vkctx.graphicsQueue, &presentInfo ) );
    }

    void Renderer::GBufferPass( ) {
        const auto &frame = vkctx.GetCurrentFrame(  );
        auto &gbuffer = vkctx.GetCurrentFrame( ).gBuffer;
        auto &albedo = vkctx.imageCodex.GetImage( gbuffer.albedo );
        auto &normal = vkctx.imageCodex.GetImage( gbuffer.normal );
        auto &position = vkctx.imageCodex.GetImage( gbuffer.position );
        auto &pbr = vkctx.imageCodex.GetImage( gbuffer.pbr );
        auto &depth = vkctx.imageCodex.GetImage( vkctx.GetCurrentFrame( ).depth );

        using namespace vk_init;
        VkClearValue clear_color = { 0.0f, 0.0f, 0.0f, 1.0f };
        std::array color_attachments = {
                AttachmentInfo( albedo.GetBaseView( ), &clear_color ),
                AttachmentInfo( normal.GetBaseView( ), &clear_color ),
                AttachmentInfo( position.GetBaseView( ), &clear_color ),
                AttachmentInfo( pbr.GetBaseView( ), &clear_color ),
        };
        VkRenderingAttachmentInfo depth_attachment =
                DepthAttachmentInfo( depth.GetBaseView( ), VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL );

        VkRenderingInfo render_info = { .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                                        .pNext = nullptr,
                                        .renderArea = VkRect2D{ VkOffset2D{ 0, 0 }, vkctx.extent },
                                        .layerCount = 1,
                                        .colorAttachmentCount = static_cast<u32>( color_attachments.size( ) ),
                                        .pColorAttachments = color_attachments.data( ),
                                        .pDepthAttachment = &depth_attachment,
                                        .pStencilAttachment = nullptr };
        vkCmdBeginRendering( frame.commandBuffer, &render_info );

        // WORK HERE


        vkCmdEndRendering( frame.commandBuffer );

    }
} // namespace TL