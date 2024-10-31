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

        PreparePbrPass( );
    }

    void Renderer::Cleanup( ) {
        m_sceneBufferGpu.reset( );
        m_gpuIbl.reset( );
        m_gpuPointLightsBuffer.reset( );
        m_gpuDirectionalLightsBuffer.reset( );
    }

    void Renderer::StartFrame( ) noexcept {
        auto &frame = vkctx->GetCurrentFrame( );

        if ( vkctx->frameNumber != 0 ) {
            VKCALL( vkWaitForFences( vkctx->device, 1, &frame.fence, true, UINT64_MAX ) );
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

        if ( vkctx->frameNumber == 0 ) {
            for ( u32 i = 0; i < vkctx->FrameOverlap; i++ ) {

                vkCmdResetQueryPool( cmd, frame.queryPoolTimestamps, 0, ( u32 )vkctx->frames[i].gpuTimestamps.size( ) );
            }
        }

        const auto &depth = vkctx->imageCodex.GetImage( frame.depth );
        depth.TransitionLayout( cmd, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, true );

        OnFrameBoundary( );
    }

    void Renderer::Frame( ) noexcept {
        auto &frame = vkctx->GetCurrentFrame( );
        ShadowMapPass( );

        SetViewportAndScissor( frame.commandBuffer );
        GBufferPass( );
        PbrPass( );

        auto cmd = frame.commandBuffer;

        // vkCmdWriteTimestamp( cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, frame.queryPoolTimestamps, 4 );
        // TL_Engine::Get( ).PbrPass( cmd );
        // vkCmdWriteTimestamp( cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, frame.queryPoolTimestamps, 5 );

        vkCmdWriteTimestamp( cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, frame.queryPoolTimestamps, 6 );
        TL_Engine::Get( ).SkyboxPass( cmd );
        vkCmdWriteTimestamp( cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, frame.queryPoolTimestamps, 7 );

        PostProcessPass( );
    }

    void Renderer::EndFrame( ) noexcept {
        const auto &frame = vkctx->GetCurrentFrame( );

        VKCALL( vkEndCommandBuffer( frame.commandBuffer ) );

        auto cmd_info = CommandBufferSubmitInfo( frame.commandBuffer );
        auto wait_info =
                SemaphoreSubmitInfo( VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, frame.swapchainSemaphore );
        auto signal_info = SemaphoreSubmitInfo( VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, frame.renderSemaphore );
        auto submit      = SubmitInfo( &cmd_info, &signal_info, &wait_info );
        VKCALL( vkQueueSubmit2( vkctx->graphicsQueue, 1, &submit, frame.fence ) );
    }

    void Renderer::Present( ) noexcept {
        const auto &frame = vkctx->GetCurrentFrame( );

        const VkPresentInfoKHR presentInfo = { .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                                               .pNext              = nullptr,
                                               // wait on _renderSemaphore, since we need the rendering to have finished
                                               // to display to the screen
                                               .waitSemaphoreCount = 1,
                                               .pWaitSemaphores    = &frame.renderSemaphore,
                                               .swapchainCount     = 1,
                                               .pSwapchains        = &vkctx->swapchain,

                                               .pImageIndices = &swapchainImageIndex };

        VKCALL( vkQueuePresentKHR( vkctx->graphicsQueue, &presentInfo ) );

        // increase frame number for next loop
        vkctx->frameNumber++;
    }
    void Renderer::UpdateScene( const Scene &scene ) {
        // 1. Parse renderable entities (meshes)

        // 2. Parse directional lights
        m_directionalLights.resize( scene.directionalLights.size( ) );
        std::ranges::transform(
                scene.directionalLights, m_directionalLights.begin( ), []( const DirectionalLight &dir_light ) {
                    GpuDirectionalLight light = { };

                    // Convert HSV to RGB and power
                    ImGui::ColorConvertHSVtoRGB( dir_light.hsv.hue, dir_light.hsv.saturation, dir_light.hsv.value,
                                                 light.color.r, light.color.g, light.color.b );
                    light.color *= dir_light.power;

                    // Transform forward vector with node transform
                    light.direction = dir_light.node->GetTransformMatrix( ) * glm::vec4( 0.0f, 0.0f, 1.0f, 0.0f );

                    // View
                    auto shadow_map_eye_pos = normalize( light.direction ) * dir_light.distance;
                    light.view              = lookAt( Vec3( shadow_map_eye_pos ), Vec3( 0.0f, 0.0f, 0.0f ), GLOBAL_UP );

                    // Projection
                    light.proj = glm::ortho( -dir_light.right, dir_light.right, -dir_light.up, dir_light.up,
                                             dir_light.nearPlane, dir_light.farPlane );

                    light.shadowMap = dir_light.shadowMap;
                    return light;
                } );


        // 3. Parse point lights
        m_pointLights.resize( scene.pointLights.size( ) );
        std::ranges::transform( scene.pointLights, m_pointLights.begin( ), []( const PointLight &point_light ) {
            GpuPointLight light = { };

            // Convert HSV to RGB and power
            ImGui::ColorConvertHSVtoRGB( point_light.hsv.hue, point_light.hsv.saturation, point_light.hsv.value,
                                         light.color.r, light.color.g, light.color.b );
            light.color *= point_light.power;

            light.position = point_light.node->transform.position;

            light.quadratic = point_light.quadratic;
            light.linear    = point_light.linear;
            light.constant  = point_light.constant;

            return light;
        } );
    }

    void Renderer::OnFrameBoundary( ) noexcept {
        auto &frame  = vkctx->GetCurrentFrame( );
        auto  cmd    = frame.commandBuffer;
        auto &engine = TL_Engine::Get( );

        m_sceneBufferGpu->AdvanceFrame( );
        m_gpuIbl->AdvanceFrame( );
        m_gpuDirectionalLightsBuffer->AdvanceFrame( );
        m_gpuPointLightsBuffer->AdvanceFrame( );

        // Upload scene information
        m_gpuIbl->Upload( &iblSettings, sizeof( IblSettings ) );
        m_gpuDirectionalLightsBuffer->Upload( m_directionalLights.data( ),
                                              sizeof( GpuDirectionalLight ) * m_directionalLights.size( ) );
        m_gpuPointLightsBuffer->Upload( m_pointLights.data( ), sizeof( GpuPointLight ) * m_pointLights.size( ) );

        engine.m_sceneData.materials = vkctx->materialCodex.GetDeviceAddress( );
        m_sceneBufferGpu->Upload( &engine.m_sceneData, sizeof( GpuSceneData ) );


        // We query the timers for the frame that was previously rendered. This means that the graph
        // and stats are always TL_VkContext::FrameOverlap - 1 behind
        vkGetQueryPoolResults( vkctx->device, frame.queryPoolTimestamps, 0, ( u32 )frame.gpuTimestamps.size( ),
                               frame.gpuTimestamps.size( ) * sizeof( uint64_t ), frame.gpuTimestamps.data( ),
                               sizeof( uint64_t ), VK_QUERY_RESULT_64_BIT );

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

    void Renderer::SetViewportAndScissor( VkCommandBuffer cmd ) noexcept {
        VkViewport viewport = {
                .x = 0, .y = 0, .width = m_extent.x, .height = m_extent.y, .minDepth = 0.0f, .maxDepth = 1.0f };
        vkCmdSetViewport( cmd, 0, 1, &viewport );

        const VkRect2D scissor = {
                .offset = { .x = 0, .y = 0 },
                .extent = { .width = static_cast<u32>( m_extent.x ), .height = static_cast<u32>( m_extent.y ) } };
        vkCmdSetScissor( cmd, 0, 1, &scissor );
    }
    void Renderer::PreparePbrPass( ) {
        DescriptorLayoutBuilder layout_builder;
        layout_builder.AddBinding( 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER );
        layout_builder.AddBinding( 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER );
        layout_builder.AddBinding( 2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER );
        m_pbrSetLayout = layout_builder.Build( vkctx->device, VK_SHADER_STAGE_FRAGMENT_BIT );

        m_pbrSet = vkctx->AllocateMultiSet( m_pbrSetLayout );

        m_gpuIbl = std::make_shared<Buffer>( BufferType::TConstant, sizeof( IblSettings ), TL_VkContext::FrameOverlap,
                                             nullptr, "[TL] Ibl Settings" );
        m_gpuDirectionalLightsBuffer =
                std::make_shared<Buffer>( BufferType::TConstant, sizeof( GpuDirectionalLight ) * 10,
                                          TL_VkContext::FrameOverlap, nullptr, "[TL] Directional Lights" );

        m_gpuPointLightsBuffer = std::make_shared<Buffer>( BufferType::TConstant, sizeof( GpuPointLight ) * 10,
                                                           TL_VkContext::FrameOverlap, nullptr, "[TL] Point Lights" );
    }

    void Renderer::GBufferPass( ) {
        const auto &frame = vkctx->GetCurrentFrame( );
        auto        cmd   = frame.commandBuffer;

        auto &gbuffer  = vkctx->GetCurrentFrame( ).gBuffer;
        auto &albedo   = vkctx->imageCodex.GetImage( gbuffer.albedo );
        auto &normal   = vkctx->imageCodex.GetImage( gbuffer.normal );
        auto &position = vkctx->imageCodex.GetImage( gbuffer.position );
        auto &pbr      = vkctx->imageCodex.GetImage( gbuffer.pbr );
        auto &depth    = vkctx->imageCodex.GetImage( vkctx->GetCurrentFrame( ).depth );

        auto pipeline = vkctx->GetOrCreatePipeline( PipelineConfig{
                .name               = "gbuffer",
                .vertex             = "../shaders/gbuffer.vert.spv",
                .pixel              = "../shaders/gbuffer.frag.spv",
                .colorTargets       = { { .format = albedo.GetFormat( ), .blendType = PipelineConfig::BlendType::OFF },
                                        { .format = normal.GetFormat( ), .blendType = PipelineConfig::BlendType::OFF },
                                        { .format = position.GetFormat( ), .blendType = PipelineConfig::BlendType::OFF },
                                        { .format = pbr.GetFormat( ), .blendType = PipelineConfig::BlendType::OFF } },
                .pushConstantRanges = { { .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                          .offset     = 0,
                                          .size       = sizeof( MeshPushConstants ) } },

                .descriptorSetLayouts = { vkctx->GetBindlessLayout( ) },
        } );

        VkClearValue clear_color       = { 0.0f, 0.0f, 0.0f, 1.0f };
        std::array   color_attachments = {
                AttachmentInfo( albedo.GetBaseView( ), &clear_color ),
                AttachmentInfo( normal.GetBaseView( ), &clear_color ),
                AttachmentInfo( position.GetBaseView( ), &clear_color ),
                AttachmentInfo( pbr.GetBaseView( ), &clear_color ),
        };
        VkRenderingAttachmentInfo depth_attachment =
                DepthAttachmentInfo( depth.GetBaseView( ), VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL );

        VkRenderingInfo render_info = { .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
                                        .pNext                = nullptr,
                                        .renderArea           = VkRect2D{ VkOffset2D{ 0, 0 }, vkctx->extent },
                                        .layerCount           = 1,
                                        .colorAttachmentCount = static_cast<u32>( color_attachments.size( ) ),
                                        .pColorAttachments    = color_attachments.data( ),
                                        .pDepthAttachment     = &depth_attachment,
                                        .pStencilAttachment   = nullptr };

        START_LABEL( cmd, "GBuffer Pass", Vec4( 1.0f, 1.0f, 0.0f, 1.0 ) );
        vkCmdBeginRendering( cmd, &render_info );
        vkCmdWriteTimestamp( cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, frame.queryPoolTimestamps, 2 );

        auto &engine = TL_Engine::Get( );

        vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetVkResource( ) );
        const auto bindless_set = vkctx->GetBindlessSet( );
        vkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetLayout( ), 0, 1, &bindless_set, 0,
                                 nullptr );

        for ( const auto &draw_command : engine.m_drawCommands ) {
            vkCmdBindIndexBuffer( cmd, draw_command.indexBuffer, 0, VK_INDEX_TYPE_UINT32 );

            MeshPushConstants push_constants = {
                    .worldFromLocal      = draw_command.worldFromLocal,
                    .sceneDataAddress    = m_sceneBufferGpu->GetDeviceAddress( ),
                    .vertexBufferAddress = draw_command.vertexBufferAddress,
                    .materialId          = draw_command.materialId,
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
        auto        cmd   = frame.commandBuffer;
        START_LABEL( cmd, "ShadowMap Pass", Vec4( 0.0f, 1.0f, 0.0f, 1.0f ) );
        vkCmdWriteTimestamp( cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, frame.queryPoolTimestamps, 0 );

        if ( vkctx->frameNumber != 0 ) {
            vkCmdWriteTimestamp( cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, frame.queryPoolTimestamps, 1 );
            END_LABEL( cmd );
            return;
        }

        auto pipeline = vkctx->GetOrCreatePipeline( PipelineConfig{
                .name               = "shadowmap",
                .vertex             = "../shaders/shadowmap.vert.spv",
                .pixel              = "../shaders/shadowmap.frag.spv",
                .cullMode           = VK_CULL_MODE_FRONT_BIT,
                .pushConstantRanges = { { .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                          .offset     = 0,
                                          .size       = sizeof( ShadowMapPushConstants ) } },

                .descriptorSetLayouts = { vkctx->GetBindlessLayout( ) },
        } );

        auto &engine = TL_Engine::Get( );

        for ( const auto &light : engine.m_gpuDirectionalLights ) {
            auto &target_image = vkctx->imageCodex.GetImage( light.shadowMap );

            VkRenderingAttachmentInfo depth_attachment =
                    DepthAttachmentInfo( target_image.GetBaseView( ), VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL );
            VkRenderingInfo render_info = { .sType      = VK_STRUCTURE_TYPE_RENDERING_INFO,
                                            .pNext      = nullptr,
                                            .renderArea = VkRect2D{ VkOffset2D{ 0, 0 }, VkExtent2D{ 2048, 2048 } },
                                            .layerCount = 1,
                                            .colorAttachmentCount = 0,
                                            .pColorAttachments    = nullptr,
                                            .pDepthAttachment     = &depth_attachment,
                                            .pStencilAttachment   = nullptr };
            vkCmdBeginRendering( cmd, &render_info );

            vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetVkResource( ) );
            const auto bindless_set = vkctx->GetBindlessSet( );
            vkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetLayout( ), 0, 1, &bindless_set,
                                     0, nullptr );

            const VkViewport viewport = { .x        = 0,
                                          .y        = 0,
                                          .width    = static_cast<float>( target_image.GetExtent( ).width ),
                                          .height   = static_cast<float>( target_image.GetExtent( ).height ),
                                          .minDepth = 0.0f,
                                          .maxDepth = 1.0f };
            vkCmdSetViewport( cmd, 0, 1, &viewport );
            const VkRect2D scissor = { .offset = { .x = 0, .y = 0 },
                                       .extent = { .width  = target_image.GetExtent( ).width,
                                                   .height = target_image.GetExtent( ).height } };
            vkCmdSetScissor( cmd, 0, 1, &scissor );

            for ( const auto &draw_command : engine.m_shadowMapCommands ) {
                vkCmdBindIndexBuffer( cmd, draw_command.indexBuffer, 0, VK_INDEX_TYPE_UINT32 );

                ShadowMapPushConstants push_constants = {
                        .projection          = light.proj,
                        .view                = light.view,
                        .model               = draw_command.worldFromLocal,
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

    void Renderer::PbrPass( ) {
        auto       &engine  = TL_Engine::Get( );
        const auto &frame   = vkctx->GetCurrentFrame( );
        auto        cmd     = frame.commandBuffer;
        const auto &gbuffer = frame.gBuffer;
        auto       &hdr     = vkctx->imageCodex.GetImage( frame.hdrColor );

        auto pipeline = vkctx->GetOrCreatePipeline( PipelineConfig{
                .name                 = "pbr",
                .vertex               = "../shaders/fullscreen_tri.vert.spv",
                .pixel                = "../shaders/pbr.frag.spv",
                .cullMode             = VK_CULL_MODE_FRONT_BIT,
                .frontFace            = VK_FRONT_FACE_COUNTER_CLOCKWISE,
                .depthTest            = false,
                .colorTargets         = { { .format = hdr.GetFormat( ), .blendType = PipelineConfig::BlendType::OFF } },
                .pushConstantRanges   = { { .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                            .offset     = 0,
                                            .size       = sizeof( PbrPushConstants ) } },
                .descriptorSetLayouts = { vkctx->GetBindlessLayout( ), m_pbrSetLayout } } );

        VkClearValue              clear_color      = { 0.0f, 0.0f, 0.0f, 0.0f };
        VkRenderingAttachmentInfo color_attachment = AttachmentInfo( hdr.GetBaseView( ), &clear_color );
        VkRenderingInfo           render_info      = { .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
                                                       .pNext                = nullptr,
                                                       .renderArea           = VkRect2D{ VkOffset2D{ 0, 0 }, vkctx->extent },
                                                       .layerCount           = 1,
                                                       .colorAttachmentCount = 1,
                                                       .pColorAttachments    = &color_attachment,
                                                       .pDepthAttachment     = nullptr,
                                                       .pStencilAttachment   = nullptr };
        START_LABEL( cmd, "PBR Pass", Vec4( 1.0f, 0.0f, 1.0f, 1.0f ) );
        vkCmdWriteTimestamp( cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, frame.queryPoolTimestamps, 4 );
        vkCmdBeginRendering( cmd, &render_info );

        vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetVkResource( ) );

        auto bindless_set = vkctx->GetBindlessSet( );
        vkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetLayout( ), 0, 1, &bindless_set, 0,
                                 nullptr );

        // Update all lights and ibl descriptor set to the current frame offset
        DescriptorWriter writer;
        writer.WriteBuffer( 0, m_gpuIbl->GetVkResource( ), sizeof( IblSettings ), m_gpuIbl->GetCurrentOffset( ),
                            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER );
        writer.WriteBuffer( 1, m_gpuDirectionalLightsBuffer->GetVkResource( ), sizeof( GpuDirectionalLight ) * 10,
                            m_gpuDirectionalLightsBuffer->GetCurrentOffset( ), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER );
        writer.WriteBuffer( 2, m_gpuPointLightsBuffer->GetVkResource( ), sizeof( GpuPointLight ) * 10,
                            m_gpuPointLightsBuffer->GetCurrentOffset( ), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER );
        writer.UpdateSet( vkctx->device, m_pbrSet.GetCurrentFrame( ) );

        auto set = m_pbrSet.GetCurrentFrame( );
        vkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetLayout( ), 1, 1, &set, 0, nullptr );

        PbrPushConstants push_constants = { .sceneDataAddress = m_sceneBufferGpu->GetDeviceAddress( ),
                                            .albedoTex        = gbuffer.albedo,
                                            .normalTex        = gbuffer.normal,
                                            .positionTex      = gbuffer.position,
                                            .pbrTex           = gbuffer.pbr,
                                            .irradianceTex    = engine.m_ibl.GetIrradiance( ),
                                            .radianceTex      = engine.m_ibl.GetRadiance( ),
                                            .brdfLut          = engine.m_ibl.GetBrdf( ) };
        vkCmdPushConstants( cmd, pipeline->GetLayout( ), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                            sizeof( PbrPushConstants ), &push_constants );

        vkCmdDraw( cmd, 3, 1, 0, 0 );

        vkCmdEndRendering( cmd );
        vkCmdWriteTimestamp( cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, frame.queryPoolTimestamps, 5 );
        END_LABEL( cmd );
    }

    void Renderer::PostProcessPass( ) {
        auto &frame = vkctx->GetCurrentFrame( );
        auto  cmd   = frame.commandBuffer;

        auto pipeline = vkctx->GetOrCreatePipeline( TL::PipelineConfig{
                .name                 = "posprocess",
                .compute              = "../shaders/post_process.comp.spv",
                .pushConstantRanges   = { { .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                                            .offset     = 0,
                                            .size       = sizeof( PostProcessPushConstants ) } },
                .descriptorSetLayouts = { vkctx->GetBindlessLayout( ) },
        } );

        vkCmdWriteTimestamp( cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, frame.queryPoolTimestamps, 8 );

        auto &output = vkctx->imageCodex.GetImage( frame.postProcessImage );
        output.TransitionLayout( cmd, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL );

        vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->GetVkResource( ) );
        const auto bindless_set = vkctx->GetBindlessSet( );
        vkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->GetLayout( ), 0, 1, &bindless_set, 0,
                                 nullptr );

        PostProcessPushConstants push_constants = { .hdr      = frame.hdrColor,
                                                    .output   = frame.postProcessImage,
                                                    .gamma    = postProcessSettings.gamma,
                                                    .exposure = postProcessSettings.exposure };
        vkCmdPushConstants( cmd, pipeline->GetLayout( ), VK_SHADER_STAGE_COMPUTE_BIT, 0,
                            sizeof( PostProcessPushConstants ), &push_constants );

        vkCmdDispatch( cmd, ( output.GetExtent( ).width + 15 ) / 16, ( output.GetExtent( ).height + 15 ) / 16, 6 );

        vkCmdWriteTimestamp( cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, frame.queryPoolTimestamps, 9 );
    }
} // namespace TL