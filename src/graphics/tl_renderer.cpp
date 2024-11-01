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

#include <graphics/ibl.h>
#include <vk_initializers.h>

using namespace vk_init;
using namespace utils;

namespace TL {

    void Renderer::Init( SDL_Window *window, Vec2 extent ) {
        m_extent = extent;

        vkctx = std::make_unique<TL_VkContext>( window );
        vkctx->Init( );

        m_camera = std::make_shared<Camera>( Vec3{ 0.0f, 0.0f, 0.0f }, 0, 0, extent.x, extent.y );

        m_sceneBufferGpu = std::make_shared<Buffer>( BufferType::TConstant, sizeof( GpuSceneData ),
                                                     TL_VkContext::FrameOverlap, nullptr, "Scene Buffer" );

        m_ibl = std::make_unique<Ibl>( );
        m_ibl->Init( *vkctx, "../../assets/texture/ibls/belfast_sunset_4k.hdr" );

        PreparePbrPass( );
        PrepareSkyboxPass( );
    }

    void Renderer::Cleanup( ) {
        m_ibl->Clean( *vkctx );

        m_sceneBufferGpu.reset( );
        m_gpuIbl.reset( );
        m_gpuPointLightsBuffer.reset( );
        m_gpuDirectionalLightsBuffer.reset( );

        vkDestroyDescriptorSetLayout( vkctx->device, m_pbrSetLayout, nullptr );
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
        auto cmd_begin_info = CommandBufferBeginInfo( VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT );
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
        SkyboxPass( );

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
        // 1. Parse renderable entities (must have atleast 1 mesh)
        m_renderables.clear( );

        // TODO: is this performant ?
        std::function<void( const std::shared_ptr<Node> & )> parse_node =
                [&]( const std::shared_ptr<Node> &node ) -> void {
            if ( !node->meshAssets.empty( ) ) {
                m_renderables.push_back( node );
            }

            for ( const auto &child : node->children ) {
                parse_node( child );
            }
        };
        for ( const auto &top_node : scene.topNodes ) {
            parse_node( top_node );
        }

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

        // 4. Update scene
        m_sceneData.view                      = m_camera->GetViewMatrix( );
        m_sceneData.proj                      = m_camera->GetProjectionMatrix( );
        m_sceneData.viewproj                  = m_sceneData.proj * m_sceneData.view;
        m_sceneData.cameraPosition            = Vec4( m_camera->GetPosition( ), 0.0f );
        m_sceneData.numberOfDirectionalLights = static_cast<int>( m_directionalLights.size( ) );
        m_sceneData.numberOfPointLights       = static_cast<int>( m_pointLights.size( ) );
    }

    void Renderer::OnFrameBoundary( ) noexcept {
        auto &frame = vkctx->GetCurrentFrame( );
        auto  cmd   = frame.commandBuffer;

        CreateDrawCommands( );

        m_sceneBufferGpu->AdvanceFrame( );
        m_gpuIbl->AdvanceFrame( );
        m_gpuDirectionalLightsBuffer->AdvanceFrame( );
        m_gpuPointLightsBuffer->AdvanceFrame( );

        // Upload scene information
        m_gpuIbl->Upload( &iblSettings, sizeof( IblSettings ) );
        m_gpuDirectionalLightsBuffer->Upload( m_directionalLights.data( ),
                                              sizeof( GpuDirectionalLight ) * m_directionalLights.size( ) );
        m_gpuPointLightsBuffer->Upload( m_pointLights.data( ), sizeof( GpuPointLight ) * m_pointLights.size( ) );

        m_sceneData.materials = vkctx->materialCodex.GetDeviceAddress( );
        m_sceneBufferGpu->Upload( &m_sceneData, sizeof( GpuSceneData ) );


        // We query the timers for the frame that was previously rendered. This means that the graph
        // and stats are always TL_VkContext::FrameOverlap - 1 behind
        vkGetQueryPoolResults( vkctx->device, frame.queryPoolTimestamps, 0, ( u32 )frame.gpuTimestamps.size( ),
                               frame.gpuTimestamps.size( ) * sizeof( uint64_t ), frame.gpuTimestamps.data( ),
                               sizeof( uint64_t ), VK_QUERY_RESULT_64_BIT );

        // Reset gpu query timers
        vkCmdResetQueryPool( cmd, frame.queryPoolTimestamps, 0, ( u32 )frame.gpuTimestamps.size( ) );

        auto time = vkctx->GetTimestampInMs( frame.gpuTimestamps.at( 0 ), frame.gpuTimestamps.at( 1 ) ) / 1000.0f;
        g_visualProfiler.AddTimer( "ShadowMap", time, utils::TaskType::Gpu );

        time = vkctx->GetTimestampInMs( frame.gpuTimestamps.at( 2 ), frame.gpuTimestamps.at( 3 ) ) / 1000.0f;
        g_visualProfiler.AddTimer( "GBuffer", time, utils::TaskType::Gpu );

        time = vkctx->GetTimestampInMs( frame.gpuTimestamps.at( 4 ), frame.gpuTimestamps.at( 5 ) ) / 1000.0f;
        g_visualProfiler.AddTimer( "Lighting", time, utils::TaskType::Gpu );

        time = vkctx->GetTimestampInMs( frame.gpuTimestamps.at( 6 ), frame.gpuTimestamps.at( 7 ) ) / 1000.0f;
        g_visualProfiler.AddTimer( "Skybox", time, utils::TaskType::Gpu );

        time = vkctx->GetTimestampInMs( frame.gpuTimestamps.at( 8 ), frame.gpuTimestamps.at( 9 ) ) / 1000.0f;
        g_visualProfiler.AddTimer( "Post Process", time, utils::TaskType::Gpu );
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

    void Renderer::PrepareSkyboxPass( ) {
        const std::vector<Mesh::Vertex> vertices = { // Front face
                                                     { { -1.0f, 1.0f, 1.0f },
                                                       0.0f,
                                                       { 0.0f, 0.0f, 1.0f },
                                                       0.0f,
                                                       { 1.0f, 2.0f, 3.0f },
                                                       0.0f,
                                                       { 4.0f, 5.0f, 6.0f },
                                                       0.0f },
                                                     { { 1.0f, 1.0f, 1.0f },
                                                       1.0f,
                                                       { 0.0f, 0.0f, 1.0f },
                                                       0.0f,
                                                       { 1.0f, 0.0f, 0.0f },
                                                       0.0f,
                                                       { 0.0f, 0.0f, 0.0f },
                                                       0.0f },
                                                     { { 1.0f, -1.0f, 1.0f },
                                                       1.0f,
                                                       { 0.0f, 0.0f, 1.0f },
                                                       1.0f,
                                                       { 1.0f, 0.0f, 0.0f },
                                                       0.0f,
                                                       { 0.0f, 0.0f, 0.0f },
                                                       0.0f },
                                                     { { -1.0f, -1.0f, 1.0f },
                                                       0.0f,
                                                       { 0.0f, 0.0f, 1.0f },
                                                       1.0f,
                                                       { 1.0f, 0.0f, 0.0f },
                                                       0.0f,
                                                       { 0.0f, 0.0f, 0.0f },
                                                       0.0f },

                                                     // Back face
                                                     { { -1.0f, 1.0f, -1.0f },
                                                       1.0f,
                                                       { 0.0f, 0.0f, -1.0f },
                                                       0.0f,
                                                       { -1.0f, 0.0f, 0.0f },
                                                       0.0f,
                                                       { 0.0f, 0.0f, 0.0f },
                                                       0.0f },
                                                     { { 1.0f, 1.0f, -1.0f },
                                                       0.0f,
                                                       { 0.0f, 0.0f, -1.0f },
                                                       0.0f,
                                                       { -1.0f, 0.0f, 0.0f },
                                                       0.0f,
                                                       { 0.0f, 0.0f, 0.0f },
                                                       0.0f },
                                                     { { 1.0f, -1.0f, -1.0f },
                                                       0.0f,
                                                       { 0.0f, 0.0f, -1.0f },
                                                       1.0f,
                                                       { -1.0f, 0.0f, 0.0f },
                                                       0.0f,
                                                       { 0.0f, 0.0f, 0.0f },
                                                       0.0f },
                                                     { { -1.0f, -1.0f, -1.0f },
                                                       1.0f,
                                                       { 0.0f, 0.0f, -1.0f },
                                                       1.0f,
                                                       { -1.0f, 0.0f, 0.0f },
                                                       0.0f,
                                                       { 0.0f, 0.0f, 0.0f },
                                                       0.0f } };

        const std::vector<uint32_t> indices = { // Front face
                                                0, 1, 2, 2, 3, 0,

                                                // Right face
                                                1, 5, 6, 6, 2, 1,

                                                // Back face
                                                5, 4, 7, 7, 6, 5,

                                                // Left face
                                                4, 0, 3, 3, 7, 4,

                                                // Top face
                                                4, 5, 1, 1, 0, 4,

                                                // Bottom face
                                                3, 2, 6, 6, 7, 3 };

        const Mesh mesh = {
                .vertices = vertices,
                .indices  = { indices },
        };
        m_skyboxMesh = vkctx->meshCodex.AddMesh( *vkctx, mesh );
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
                .cullMode           = VK_CULL_MODE_NONE,
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

        vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetVkResource( ) );
        const auto bindless_set = vkctx->GetBindlessSet( );
        vkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetLayout( ), 0, 1, &bindless_set, 0,
                                 nullptr );

        for ( const auto &draw_command : m_drawCommands ) {
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

        for ( const auto &light : m_directionalLights ) {
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

            for ( const auto &draw_command : m_shadowMapCommands ) {
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
                                            .irradianceTex    = m_ibl->GetIrradiance( ),
                                            .radianceTex      = m_ibl->GetRadiance( ),
                                            .brdfLut          = m_ibl->GetBrdf( ) };
        vkCmdPushConstants( cmd, pipeline->GetLayout( ), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                            sizeof( PbrPushConstants ), &push_constants );

        vkCmdDraw( cmd, 3, 1, 0, 0 );

        vkCmdEndRendering( cmd );
        vkCmdWriteTimestamp( cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, frame.queryPoolTimestamps, 5 );
        END_LABEL( cmd );
    }

    void Renderer::SkyboxPass( ) {
        auto &frame = vkctx->GetCurrentFrame( );
        auto  cmd   = frame.commandBuffer;
        auto &hdr   = vkctx->imageCodex.GetImage( frame.hdrColor );
        auto &depth = vkctx->imageCodex.GetImage( frame.depth );

        auto pipeline = vkctx->GetOrCreatePipeline( PipelineConfig{
                .name                 = "skybox",
                .vertex               = "../shaders/skybox.vert.spv",
                .pixel                = "../shaders/skybox.frag.spv",
                .cullMode             = VK_CULL_MODE_NONE,
                .depthWrite           = false,
                .colorTargets         = { { .format = hdr.GetFormat( ), .blendType = PipelineConfig::BlendType::OFF } },
                .pushConstantRanges   = { { .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                            .offset     = 0,
                                            .size       = sizeof( SkyboxPushConstants ) } },
                .descriptorSetLayouts = { vkctx->GetBindlessLayout( ) } } );

        START_LABEL( cmd, "Skybox Pass", Vec4( 0.0f, 1.0f, 0.0f, 1.0f ) );
        vkCmdWriteTimestamp( cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, frame.queryPoolTimestamps, 6 );


        VkRenderingAttachmentInfo color_attachment = AttachmentInfo( hdr.GetBaseView( ), nullptr );
        VkRenderingAttachmentInfo depth_attachment = { .sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                                                       .pNext       = nullptr,
                                                       .imageView   = depth.GetBaseView( ),
                                                       .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                                                       .loadOp      = VK_ATTACHMENT_LOAD_OP_LOAD,
                                                       .storeOp     = VK_ATTACHMENT_STORE_OP_STORE };
        VkRenderingInfo           render_info      = { .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
                                                       .pNext                = nullptr,
                                                       .renderArea           = VkRect2D{ VkOffset2D{ 0, 0 }, vkctx->extent },
                                                       .layerCount           = 1,
                                                       .colorAttachmentCount = 1,
                                                       .pColorAttachments    = &color_attachment,
                                                       .pDepthAttachment     = &depth_attachment,
                                                       .pStencilAttachment   = nullptr };
        vkCmdBeginRendering( cmd, &render_info );

        vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetVkResource( ) );

        auto bindless_set = vkctx->GetBindlessSet( );
        vkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetLayout( ), 0, 1, &bindless_set, 0,
                                 nullptr );

        auto &mesh = vkctx->meshCodex.GetMesh( m_skyboxMesh );
        vkCmdBindIndexBuffer( cmd, mesh.indexBuffer[0]->GetVkResource( ), 0, VK_INDEX_TYPE_UINT32 );

        const SkyboxPushConstants push_constants = { .sceneDataAddress    = m_sceneBufferGpu->GetDeviceAddress( ),
                                                     .vertexBufferAddress = mesh.vertexBufferAddress,
                                                     .textureId           = m_ibl->GetSkybox( ) };
        vkCmdPushConstants( cmd, pipeline->GetLayout( ), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                            sizeof( SkyboxPushConstants ), &push_constants );

        vkCmdDrawIndexed( cmd, mesh.indexCount[0], 1, 0, 0, 0 );

        vkCmdEndRendering( cmd );
        vkCmdWriteTimestamp( cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, frame.queryPoolTimestamps, 7 );
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

    VisibilityLODResult Renderer::VisibilityCheckWithLOD( const Mat4 &transform, const AABoundingBox *aabb,
                                                          const Frustum &frustum ) {
        // If neither frustum or lod is enable, then everything is visible and finest lod
        if ( !settings.frustumCulling && !settings.lodSystem ) {
            return { true, 0 };
        }

        Vec3 points[] = {
                { aabb->min.x, aabb->min.y, aabb->min.z }, { aabb->max.x, aabb->min.y, aabb->min.z },
                { aabb->max.x, aabb->max.y, aabb->min.z }, { aabb->min.x, aabb->max.y, aabb->min.z },

                { aabb->min.x, aabb->min.y, aabb->max.z }, { aabb->max.x, aabb->min.y, aabb->max.z },
                { aabb->max.x, aabb->max.y, aabb->max.z }, { aabb->min.x, aabb->max.y, aabb->max.z },
        };

        Vec4 clips[8] = { };

        // Transform points to world space and clip space
        for ( int i = 0; i < 8; ++i ) {
            points[i] = transform * Vec4( points[i], 1.0f );

            // Only need the clip space coordinates for the LOD system
            if ( settings.lodSystem ) {
                clips[i] = m_camera->GetProjectionMatrix( ) * m_camera->GetViewMatrix( ) * Vec4( points[i], 1.0f );
            }
        }

        bool is_visible = true;

        if ( settings.frustumCulling ) {
            // for each plane…
            for ( int i = 0; i < 6; ++i ) {
                bool inside_frustum = false;

                for ( int j = 0; j < 8; ++j ) {
                    if ( dot( Vec3( points[j] ), Vec3( frustum.planes[i] ) ) + frustum.planes[i].w > 0 ) {
                        inside_frustum = true;
                        break;
                    }
                }

                if ( !inside_frustum ) {
                    is_visible = false;
                }
            }
        }

        if ( !is_visible ) {
            return { false, -1 };
        }

        if ( settings.lodSystem ) {
            float min_x = std::numeric_limits<float>::max( );
            float max_x = std::numeric_limits<float>::lowest( );
            float min_y = std::numeric_limits<float>::max( );
            float max_y = std::numeric_limits<float>::lowest( );

            for ( int i = 0; i < 8; i++ ) {
                Vec4 clip = clips[i];
                Vec3 ndc  = Vec3( clip ) / clip.w;

                ndc         = glm::clamp( ndc, -1.0f, 1.0f );
                Vec2 screen = Vec2( ( ndc.x + 1.0f ) * 0.5f * vkctx->extent.width,
                                    ( 1.0f - ndc.y ) * 0.5f * vkctx->extent.height );

                min_x = std::min( min_x, screen.x );
                max_x = std::max( max_x, screen.x );
                min_y = std::min( min_y, screen.y );
                max_y = std::max( max_y, screen.y );
            }

            float width       = max_x - min_x;
            float height      = max_y - min_y;
            float screen_size = std::max( width, height );

            constexpr float lodThresholds[5] = { 250.0f, 170.0f, 100.0f, 50.0f, 20.0f };
            int             selected_lod     = 5;
            for ( int i = 0; i < 5; i++ ) {
                if ( screen_size > lodThresholds[i] ) {
                    selected_lod = i;
                    break;
                }
            }

            return { true, selected_lod };
        }

        return { true, 0 };
    }

    void Renderer::CreateDrawCommands( ) {
        ScopedProfiler commands_task( "Create Commands", Cpu );

        m_shadowMapCommands.clear( );
        m_drawCommands.clear( );

        for ( const auto &node : m_renderables ) {
            int i = 0;
            for ( const auto mesh_asset : node->meshAssets ) {
                auto            model = node->GetTransformMatrix( );
                auto           &mesh  = vkctx->meshCodex.GetMesh( mesh_asset.mesh );
                MeshDrawCommand mdc   = {
                          .indexBuffer         = mesh.indexBuffer[0]->GetVkResource( ),
                          .indexCount          = mesh.indexCount[0],
                          .vertexBufferAddress = mesh.vertexBufferAddress,
                          .worldFromLocal      = model,
                          .materialId          = mesh_asset.material,
                };

                // TODO: dont rerender static
                m_shadowMapCommands.push_back( mdc );

                if ( settings.frustumCulling ) {
                    auto &aabb       = node->boundingBoxes[i++];
                    auto  visibility = VisibilityCheckWithLOD( model, &aabb,
                                                              settings.useFrozenFrustum ? settings.lastSavedFrustum
                                                                                         : m_camera->GetFrustum( ) );
                    if ( !visibility.isVisible ) {
                        continue;
                    }

                    // Do not update the current LOD for the node if freeze LOD system is toggled
                    if ( !settings.freezeLodSystem ) {
                        node->currentLod = std::min( static_cast<int>( mesh.indexCount.size( ) - 1 ),
                                                     visibility.lodLevelToRender );
                    }

                    mdc.indexBuffer = mesh.indexBuffer[node->currentLod]->GetVkResource( );
                    mdc.indexCount  = mesh.indexCount[node->currentLod];
                    m_drawCommands.push_back( mdc );
                }
                else {
                    m_drawCommands.push_back( mdc );
                }
            }
        }
    }
} // namespace TL