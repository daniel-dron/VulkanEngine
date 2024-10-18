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

#include "gbuffer_pipeline.h"

#include <vk_initializers.h>
#include <vk_pipelines.h>

#include "graphics/draw_command.h"
#include "graphics/gfx_device.h"
#include "vk_engine.h"

using namespace vk_init;

GBufferPipeline::Result<> GBufferPipeline::Init( GfxDevice &gfx ) {
    auto &frag_shader = gfx.shaderStorage->Get( "gbuffer", TFragment );
    auto &vert_shader = gfx.shaderStorage->Get( "gbuffer", TVertex );

    auto reconstruct_shader_callback = [&]( VkShaderModule shader ) {
        VK_CHECK( vkWaitForFences( gfx.device, 1, &gfx.swapchain.GetCurrentFrame( ).fence, true, 1000000000 ) );
        Cleanup( gfx );

        Reconstruct( gfx );
    };

    frag_shader.RegisterReloadCallback( reconstruct_shader_callback );
    vert_shader.RegisterReloadCallback( reconstruct_shader_callback );

    Reconstruct( gfx );

    return { };
}

void GBufferPipeline::Cleanup( GfxDevice &gfx ) {
    vkDestroyPipelineLayout( gfx.device, m_layout, nullptr );
    vkDestroyPipeline( gfx.device, m_pipeline, nullptr );
    gfx.Free( m_gpuSceneData );
}

DrawStats GBufferPipeline::Draw( GfxDevice &gfx, VkCommandBuffer cmd, const std::vector<MeshDrawCommand> &drawCommands, const GpuSceneData &sceneData ) const {
    DrawStats stats = { };

    GpuSceneData *gpu_scene_addr = nullptr;
    vmaMapMemory( gfx.allocator, m_gpuSceneData.allocation, reinterpret_cast<void **>( &gpu_scene_addr ) );
    *gpu_scene_addr = sceneData;
    gpu_scene_addr->materials = gfx.materialCodex.GetDeviceAddress( );
    vmaUnmapMemory( gfx.allocator, m_gpuSceneData.allocation );

    vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline );

    const auto bindless_set = gfx.GetBindlessSet( );
    vkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_layout, 0, 1, &bindless_set, 0, nullptr );

    auto &target_image = gfx.imageCodex.GetImage( gfx.swapchain.GetCurrentFrame( ).gBuffer.albedo );

    VkViewport viewport = {
            .x = 0,
            .y = 0,
            .width = static_cast<float>( target_image.GetExtent( ).width ),
            .height = static_cast<float>( target_image.GetExtent( ).height ),
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
    };
    vkCmdSetViewport( cmd, 0, 1, &viewport );

    const VkRect2D scissor = {
            .offset = {
                    .x = 0,
                    .y = 0,
            },
            .extent = { .width = target_image.GetExtent( ).width, .height = target_image.GetExtent( ).height },
    };
    vkCmdSetScissor( cmd, 0, 1, &scissor );

    const VkBufferDeviceAddressInfo address_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            .pNext = nullptr,
            .buffer = m_gpuSceneData.buffer,
    };
    auto gpu_scene_address = vkGetBufferDeviceAddress( gfx.device, &address_info );

    for ( const auto &draw_command : drawCommands ) {
        vkCmdBindIndexBuffer( cmd, draw_command.indexBuffer, 0, VK_INDEX_TYPE_UINT32 );

        PushConstants push_constants = {
                .worldFromLocal = draw_command.worldFromLocal,
                .sceneDataAddress = gpu_scene_address,
                .vertexBufferAddress = draw_command.vertexBufferAddress,
                .materialId = draw_command.materialId,
        };
        vkCmdPushConstants( cmd, m_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( PushConstants ), &push_constants );

        vkCmdDrawIndexed( cmd, draw_command.indexCount, 1, 0, 0, 0 );

        stats.triangleCount += draw_command.indexCount / 3;
    }

    return stats;
}

void GBufferPipeline::Reconstruct( GfxDevice &gfx ) {
    auto &frag_shader = gfx.shaderStorage->Get( "gbuffer", TFragment );
    auto &vert_shader = gfx.shaderStorage->Get( "gbuffer", TVertex );

    VkPushConstantRange range = {
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset = 0,
            .size = sizeof( PushConstants ) };

    auto bindless_layout = gfx.GetBindlessLayout( );
    VkDescriptorSetLayout layouts[] = { bindless_layout };

    // ----------
    // pipeline
    VkPipelineLayoutCreateInfo layout_info = PipelineLayoutCreateInfo( );
    layout_info.pSetLayouts = layouts;
    layout_info.setLayoutCount = 1;
    layout_info.pPushConstantRanges = &range;
    layout_info.pushConstantRangeCount = 1;
    VK_CHECK( vkCreatePipelineLayout( gfx.device, &layout_info, nullptr, &m_layout ) );

    PipelineBuilder builder;
    builder.SetShaders( vert_shader.handle, frag_shader.handle );
    builder.SetInputTopology( VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST );
    builder.SetPolygonMode( VK_POLYGON_MODE_FILL );
    builder.SetCullMode( VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE );
    builder.SetMultisamplingNone( );
    builder.DisableBlending( );
    builder.EnableDepthTest( true, VK_COMPARE_OP_LESS );

    auto &g_buffer = gfx.swapchain.GetCurrentFrame( ).gBuffer;
    auto &albedo = gfx.imageCodex.GetImage( g_buffer.albedo );
    auto &normal = gfx.imageCodex.GetImage( g_buffer.normal );
    auto &position = gfx.imageCodex.GetImage( g_buffer.position );
    auto &pbr = gfx.imageCodex.GetImage( g_buffer.pbr );
    auto &depth = gfx.imageCodex.GetImage( gfx.swapchain.GetCurrentFrame( ).depth );

    std::array formats = {
            albedo.GetFormat( ),
            normal.GetFormat( ),
            position.GetFormat( ),
            pbr.GetFormat( ) };
    builder.SetColorAttachmentFormats( formats.data( ), formats.size( ) );
    builder.SetDepthFormat( depth.GetFormat( ) );
    builder.SetLayout( m_layout );
    m_pipeline = builder.Build( gfx.device );

#ifdef ENABLE_DEBUG_UTILS
    const VkDebugUtilsObjectNameInfoEXT obj = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
            .pNext = nullptr,
            .objectType = VK_OBJECT_TYPE_PIPELINE,
            .objectHandle = reinterpret_cast<uint64_t>( m_pipeline ),
            .pObjectName = "GBuffer Pipeline",
    };
    vkSetDebugUtilsObjectNameEXT( gfx.device, &obj );
#endif

    m_gpuSceneData = gfx.Allocate( sizeof( GpuSceneData ), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, "Scene Data GBuffer Pipeline" );
}
