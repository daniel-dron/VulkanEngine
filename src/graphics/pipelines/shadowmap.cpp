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
#include <vk_pipelines.h>
#include "shadowmap.h"

#include "graphics/draw_command.h"
#include "graphics/gfx_device.h"
#include "vk_engine.h"

using namespace vk_init;

ShadowMap::Result<> ShadowMap::Init( GfxDevice &gfx ) {
    auto &frag_shader = gfx.shaderStorage->Get( "shadowmap", TFragment );
    auto &vert_shader = gfx.shaderStorage->Get( "shadowmap", TVertex );

    auto reconstruct_shader_callback = [&]( VkShaderModule shader ) {
        VKCALL( vkWaitForFences( gfx.device, 1, &gfx.swapchain.GetCurrentFrame( ).fence, true, 1000000000 ) );
        Cleanup( gfx );

        Reconstruct( gfx );
    };

    frag_shader.RegisterReloadCallback( reconstruct_shader_callback );
    vert_shader.RegisterReloadCallback( reconstruct_shader_callback );

    Reconstruct( gfx );

    return { };
}

void ShadowMap::Cleanup( GfxDevice &gfx ) {
    vkDestroyPipelineLayout( gfx.device, m_layout, nullptr );
    vkDestroyPipeline( gfx.device, m_pipeline, nullptr );
}

DrawStats ShadowMap::Draw( GfxDevice &gfx, VkCommandBuffer cmd, const std::vector<MeshDrawCommand> &drawCommands,
                           const std::vector<GpuDirectionalLight> &lights ) const {
    DrawStats stats = { };

    for ( const auto &light : lights ) {
        auto &target_image = gfx.imageCodex.GetImage( light.shadowMap );

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

        vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline );

        const VkViewport viewport = {
                .x = 0,
                .y = 0,
                .width = static_cast<float>( target_image.GetExtent( ).width ),
                .height = static_cast<float>( target_image.GetExtent( ).height ),
                .minDepth = 0.0f,
                .maxDepth = 1.0f,
        };
        vkCmdSetViewport( cmd, 0, 1, &viewport );

        const VkRect2D scissor = {
                .offset =
                        {
                                .x = 0,
                                .y = 0,
                        },
                .extent =
                        {
                                .width = target_image.GetExtent( ).width,
                                .height = target_image.GetExtent( ).height,
                        },
        };
        vkCmdSetScissor( cmd, 0, 1, &scissor );

        for ( const auto &draw_command : drawCommands ) {
            vkCmdBindIndexBuffer( cmd, draw_command.indexBuffer, 0, VK_INDEX_TYPE_UINT32 );

            PushConstants push_constants = {
                    .projection = light.proj,
                    .view = light.view,
                    .model = draw_command.worldFromLocal,
                    .vertexBufferAddress = draw_command.vertexBufferAddress,
            };
            vkCmdPushConstants( cmd, m_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                                sizeof( PushConstants ), &push_constants );

            vkCmdDrawIndexed( cmd, draw_command.indexCount, 1, 0, 0, 0 );

            stats.drawcallCount++;
            stats.triangleCount += draw_command.indexCount / 3;
        }

        vkCmdEndRendering( cmd );
    }

    return stats;
}

void ShadowMap::Reconstruct( GfxDevice &gfx ) {
    auto &frag_shader = gfx.shaderStorage->Get( "shadowmap", TFragment );
    auto &vert_shader = gfx.shaderStorage->Get( "shadowmap", TVertex );

    VkPushConstantRange range = { .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                  .offset = 0,
                                  .size = sizeof( PushConstants ) };

    VkPipelineLayoutCreateInfo layout_info = PipelineLayoutCreateInfo( );
    layout_info.pSetLayouts = nullptr;
    layout_info.setLayoutCount = 0;
    layout_info.pPushConstantRanges = &range;
    layout_info.pushConstantRangeCount = 1;
    VKCALL( vkCreatePipelineLayout( gfx.device, &layout_info, nullptr, &m_layout ) );

    PipelineBuilder builder;
    builder.SetShaders( vert_shader.handle, frag_shader.handle );
    builder.SetInputTopology( VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST );
    builder.SetPolygonMode( VK_POLYGON_MODE_FILL );
    builder.SetCullMode( VK_CULL_MODE_FRONT_BIT, VK_FRONT_FACE_CLOCKWISE );
    builder.SetMultisamplingNone( );
    builder.DisableBlending( );
    builder.EnableDepthTest( true, VK_COMPARE_OP_LESS );

    VkFormat format = gfx.imageCodex.GetImage( gfx.swapchain.GetCurrentFrame( ).depth ).GetFormat( );
    builder.SetDepthFormat( format );

    builder.SetLayout( m_layout );
    m_pipeline = builder.Build( gfx.device );

#ifdef ENABLE_DEBUG_UTILS
    const VkDebugUtilsObjectNameInfoEXT obj = { .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
                                                .pNext = nullptr,
                                                .objectType = VK_OBJECT_TYPE_PIPELINE,
                                                .objectHandle = reinterpret_cast<uint64_t>( m_pipeline ),
                                                .pObjectName = "ShadowMap Pipeline" };
    vkSetDebugUtilsObjectNameEXT( gfx.device, &obj );
#endif
}
