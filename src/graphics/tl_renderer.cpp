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

using namespace vk_init;

namespace TL {

    void Renderer::Init( SDL_Window *window, Vec2 extent ) {
        m_extent = extent;

        vkctx = std::make_unique<TL_VkContext>( window );

        m_camera = std::make_shared<Camera>( Vec3{ 0.0f, 0.0f, 0.0f }, 0, 0, extent.x, extent.y );

        m_sceneBufferGpu = std::make_shared<Buffer>( BufferType::TConstant, sizeof( GpuSceneData ),
                                                     TL_VkContext::FrameOverlap, nullptr, "Scene Buffer" );
    }

    void Renderer::Cleanup( ) { m_sceneBufferGpu.reset( ); }

    void Renderer::StartFrame( ) noexcept {
        auto &frame = vkctx->GetCurrentFrame( );

        if ( vkctx->frameNumber != 0 ) {
            VKCALL( vkWaitForFences( vkctx->device, 1, &frame.fence, true, UINT64_MAX ) );

            OnFrameBoundary( );
        }

        // Reset fence in case the next KHR image acquire fails or we lose device
        VKCALL( vkResetFences( vkctx->device, 1, &frame.fence ) );

        VKCALL( vkAcquireNextImageKHR( vkctx->device, vkctx->swapchain, UINT64_MAX, frame.swapchainSemaphore, nullptr,
                                       &swapchainImageIndex ) );

        // Start command recording (for now we use one single command buffer for the entire main passes)
        auto cmd = frame.commandBuffer;
        VKCALL( vkResetCommandBuffer( cmd, 0 ) );
        auto cmd_begin_info = vk_init::CommandBufferBeginInfo( VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT );
        VKCALL( vkBeginCommandBuffer( cmd, &cmd_begin_info ) );


        // Query GPU timers from last frame
        if ( vkctx->frameNumber != 0 ) {
            vkGetQueryPoolResults( vkctx->device, frame.queryPoolTimestamps, 0, ( u32 )frame.gpuTimestamps.size( ),
                                   frame.gpuTimestamps.size( ) * sizeof( uint64_t ), frame.gpuTimestamps.data( ),
                                   sizeof( uint64_t ), VK_QUERY_RESULT_64_BIT ); //
        }

        // Reset gpu query timers
        vkCmdResetQueryPool( cmd, frame.queryPoolTimestamps, 0, ( u32 )frame.gpuTimestamps.size( ) );

        auto time = vkctx->GetTimestampInMs( frame.gpuTimestamps.at( 0 ), frame.gpuTimestamps.at( 1 ) ) / 1000.0f;
        g_visualProfiler.AddTimer( "ShadowMap", time, utils::VisualProfiler::Gpu );

        time = vkctx->GetTimestampInMs( frame.gpuTimestamps.at( 2 ), frame.gpuTimestamps.at( 3 ) ) / 1000.0f;
        g_visualProfiler.AddTimer( "GBuffer", time, utils::VisualProfiler::Gpu );

        time = vkctx->GetTimestampInMs( frame.gpuTimestamps.at( 4 ), frame.gpuTimestamps.at( 5 ) ) / 1000.0f;
        g_visualProfiler.AddTimer( "Lighting", time, utils::VisualProfiler::Gpu );

        time = vkctx->GetTimestampInMs( frame.gpuTimestamps.at( 6 ), frame.gpuTimestamps.at( 7 ) ) / 1000.0f;
        g_visualProfiler.AddTimer( "Skybox", time, utils::VisualProfiler::Gpu );

        time = vkctx->GetTimestampInMs( frame.gpuTimestamps.at( 8 ), frame.gpuTimestamps.at( 9 ) ) / 1000.0f;
        g_visualProfiler.AddTimer( "Post Process", time, utils::VisualProfiler::Gpu );
    }

    void Renderer::Frame( ) noexcept {
        auto &frame = vkctx->GetCurrentFrame( );
        ShadowMapPass( );

        SetViewportAndScissor( frame.commandBuffer );
        GBufferPass( );
    }

    void Renderer::EndFrame( ) noexcept {
        const auto &frame = vkctx->GetCurrentFrame( );

        VKCALL( vkEndCommandBuffer( frame.commandBuffer ) );

        auto cmd_info = vk_init::CommandBufferSubmitInfo( frame.commandBuffer );
        auto wait_info = vk_init::SemaphoreSubmitInfo( VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
                                                       frame.swapchainSemaphore );
        auto signal_info = vk_init::SemaphoreSubmitInfo( VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, frame.renderSemaphore );
        auto submit = vk_init::SubmitInfo( &cmd_info, &signal_info, &wait_info );
        VKCALL( vkQueueSubmit2( vkctx->graphicsQueue, 1, &submit, frame.fence ) );
    }

    void Renderer::Present( ) noexcept {
        const auto &frame = vkctx->GetCurrentFrame( );

        const VkPresentInfoKHR presentInfo = { .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                                               .pNext = nullptr,
                                               // wait on _renderSemaphore, since we need the rendering to have finished
                                               // to display to the screen
                                               .waitSemaphoreCount = 1,
                                               .pWaitSemaphores = &frame.renderSemaphore,

                                               .swapchainCount = 1,
                                               .pSwapchains = &vkctx->swapchain,

                                               .pImageIndices = &swapchainImageIndex };

        VKCALL( vkQueuePresentKHR( vkctx->graphicsQueue, &presentInfo ) );
    }

    void Renderer::OnFrameBoundary( ) noexcept {
        // TODO: move this
        m_sceneBufferGpu->AdvanceFrame( );
    }

    void Renderer::SetViewportAndScissor( VkCommandBuffer cmd ) noexcept {
        VkViewport viewport = { .x = 0,
                                .y = 0,
                                .width = static_cast<float>( m_extent.x ),
                                .height = static_cast<float>( m_extent.y ),
                                .minDepth = 0.0f,
                                .maxDepth = 1.0f };
        vkCmdSetViewport( cmd, 0, 1, &viewport );

        const VkRect2D scissor = { .offset = { .x = 0, .y = 0 },
                                   .extent = { .width = ( u32 )m_extent.x, .height = ( u32 )m_extent.y } };
        vkCmdSetScissor( cmd, 0, 1, &scissor );
    }

    void Renderer::GBufferPass( ) {
        const auto &frame = vkctx->GetCurrentFrame( );
        auto cmd = frame.commandBuffer;

        auto &gbuffer = vkctx->GetCurrentFrame( ).gBuffer;
        auto &albedo = vkctx->imageCodex.GetImage( gbuffer.albedo );
        auto &normal = vkctx->imageCodex.GetImage( gbuffer.normal );
        auto &position = vkctx->imageCodex.GetImage( gbuffer.position );
        auto &pbr = vkctx->imageCodex.GetImage( gbuffer.pbr );
        auto &depth = vkctx->imageCodex.GetImage( vkctx->GetCurrentFrame( ).depth );

        auto pipeline = vkctx->GetOrCreatePipeline( PipelineConfig{
                .name = "gbuffer",
                .vertex = "../shaders/gbuffer.vert.spv",
                .pixel = "../shaders/gbuffer.frag.spv",
                .colorTargets = { { .format = albedo.GetFormat( ), .blendType = PipelineConfig::BlendType::OFF },
                                  { .format = normal.GetFormat( ), .blendType = PipelineConfig::BlendType::OFF },
                                  { .format = position.GetFormat( ), .blendType = PipelineConfig::BlendType::OFF },
                                  { .format = pbr.GetFormat( ), .blendType = PipelineConfig::BlendType::OFF } },
                .pushConstantRanges = { { .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                          .offset = 0,
                                          .size = sizeof( MeshPushConstants ) } },

                .descriptorSetLayouts = { vkctx->GetBindlessLayout( ) },
        } );

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
                                        .renderArea = VkRect2D{ VkOffset2D{ 0, 0 }, vkctx->extent },
                                        .layerCount = 1,
                                        .colorAttachmentCount = static_cast<u32>( color_attachments.size( ) ),
                                        .pColorAttachments = color_attachments.data( ),
                                        .pDepthAttachment = &depth_attachment,
                                        .pStencilAttachment = nullptr };

        START_LABEL( cmd, "GBuffer Pass", Vec4( 1.0f, 1.0f, 0.0f, 1.0 ) );
        vkCmdBeginRendering( cmd, &render_info );
        vkCmdWriteTimestamp( cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, frame.queryPoolTimestamps, 2 );

        auto &engine = TL_Engine::Get( );

        engine.m_sceneData.materials = vkctx->materialCodex.GetDeviceAddress( );
        m_sceneBufferGpu->Upload( &engine.m_sceneData, sizeof( GpuSceneData ) );

        vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetVkResource( ) );
        const auto bindless_set = vkctx->GetBindlessSet( );
        vkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetLayout( ), 0, 1, &bindless_set, 0,
                                 nullptr );

        for ( const auto &draw_command : engine.m_drawCommands ) {
            vkCmdBindIndexBuffer( cmd, draw_command.indexBuffer, 0, VK_INDEX_TYPE_UINT32 );

            MeshPushConstants push_constants = {
                    .worldFromLocal = draw_command.worldFromLocal,
                    .sceneDataAddress = m_sceneBufferGpu->GetDeviceAddress( ),
                    .vertexBufferAddress = draw_command.vertexBufferAddress,
                    .materialId = draw_command.materialId,
            };
            vkCmdPushConstants( cmd, pipeline->GetLayout( ), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                0, sizeof( MeshPushConstants ), &push_constants );

            vkCmdDrawIndexed( cmd, draw_command.indexCount, 1, 0, 0, 0 );
        }

        vkCmdWriteTimestamp( cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, frame.queryPoolTimestamps, 3 );
        vkCmdEndRendering( cmd );
        END_LABEL( cmd );
    }

    void Renderer::ShadowMapPass( ) {
        const auto &frame = vkctx->GetCurrentFrame( );
        auto cmd = frame.commandBuffer;
        START_LABEL( cmd, "ShadowMap Pass", Vec4( 0.0f, 1.0f, 0.0f, 1.0f ) );
        vkCmdWriteTimestamp( cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, frame.queryPoolTimestamps, 0 );

        auto pipeline = vkctx->GetOrCreatePipeline( PipelineConfig{
                .name = "shadowmap",
                .vertex = "../shaders/shadowmap.vert.spv",
                .pixel = "../shaders/shadowmap.frag.spv",
                .cullMode = VK_CULL_MODE_FRONT_BIT,
                .pushConstantRanges = { { .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                          .offset = 0,
                                          .size = sizeof( ShadowMapPushConstants ) } },

                .descriptorSetLayouts = { vkctx->GetBindlessLayout( ) },
        } );

        auto &engine = TL_Engine::Get( );

        for ( const auto &light : engine.m_gpuDirectionalLights ) {
            auto &target_image = vkctx->imageCodex.GetImage( light.shadowMap );

            VkRenderingAttachmentInfo depth_attachment =
                    DepthAttachmentInfo( target_image.GetBaseView( ), VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL );
            VkRenderingInfo render_info = { .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                                            .pNext = nullptr,
                                            .renderArea = VkRect2D{ VkOffset2D{ 0, 0 }, VkExtent2D{ 2048, 2048 } },
                                            .layerCount = 1,
                                            .colorAttachmentCount = 0,
                                            .pColorAttachments = nullptr,
                                            .pDepthAttachment = &depth_attachment,
                                            .pStencilAttachment = nullptr };
            vkCmdBeginRendering( cmd, &render_info );

            vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetVkResource( ) );

            const VkViewport viewport = { .x = 0,
                                          .y = 0,
                                          .width = static_cast<float>( target_image.GetExtent( ).width ),
                                          .height = static_cast<float>( target_image.GetExtent( ).height ),
                                          .minDepth = 0.0f,
                                          .maxDepth = 1.0f };
            vkCmdSetViewport( cmd, 0, 1, &viewport );
            const VkRect2D scissor = { .offset = { .x = 0, .y = 0 },
                                       .extent = { .width = target_image.GetExtent( ).width,
                                                   .height = target_image.GetExtent( ).height } };
            vkCmdSetScissor( cmd, 0, 1, &scissor );

            for ( const auto &draw_command : engine.m_shadowMapCommands ) {
                vkCmdBindIndexBuffer( cmd, draw_command.indexBuffer, 0, VK_INDEX_TYPE_UINT32 );

                ShadowMapPushConstants push_constants = {
                        .projection = light.proj,
                        .view = light.view,
                        .model = draw_command.worldFromLocal,
                        .vertexBufferAddress = draw_command.vertexBufferAddress,
                };
                vkCmdPushConstants( cmd, pipeline->GetLayout( ),
                                    VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                                    sizeof( ShadowMapPushConstants ), &push_constants );

                vkCmdDrawIndexed( cmd, draw_command.indexCount, 1, 0, 0, 0 );
            }

            vkCmdEndRendering( cmd );
        }

        vkCmdWriteTimestamp( cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, frame.queryPoolTimestamps, 1 );
        END_LABEL( cmd );
    }
} // namespace TL