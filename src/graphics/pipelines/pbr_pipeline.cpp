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

#include "pbr_pipeline.h"

#include <vk_initializers.h>
#include <vk_pipelines.h>

#include <imgui.h>

#include "graphics/gfx_device.h"
#include "vk_engine.h"

using namespace vk_init;

PbrPipeline::Result<> PbrPipeline::Init( GfxDevice &gfx ) {
    auto &frag_shader = gfx.shaderStorage->Get( "pbr", TFragment );

    auto reconstruct_shader_callback = [&]( VkShaderModule shader ) {
        VK_CHECK( vkWaitForFences( gfx.device, 1, &gfx.swapchain.GetCurrentFrame( ).fence, true, 1000000000 ) );
        Cleanup( gfx );

        Reconstruct( gfx );
    };

    frag_shader.RegisterReloadCallback( reconstruct_shader_callback );

    Reconstruct( gfx );

    return { };
}

void PbrPipeline::Cleanup( GfxDevice &gfx ) {
    vkDestroyPipelineLayout( gfx.device, m_layout, nullptr );
    vkDestroyPipeline( gfx.device, m_pipeline, nullptr );
    vkDestroyDescriptorSetLayout( gfx.device, m_ubLayout, nullptr );
    gfx.Free( m_gpuSceneData );
    gfx.Free( m_gpuIbl );
    gfx.Free( m_gpuDirectionalLights );
    gfx.Free( m_gpuPointLights );
}

DrawStats PbrPipeline::Draw( GfxDevice &gfx, VkCommandBuffer cmd, const GpuSceneData &sceneData, const std::vector<GpuDirectionalLight> &directionalLights, const std::vector<GpuPointLightData> &pointLights, const GBuffer &gBuffer, uint32_t irradianceMap, uint32_t radianceMap, uint32_t brdfLut ) const {
    DrawStats stats = { };

    GpuSceneData *gpu_scene_addr = nullptr;
    vmaMapMemory( gfx.allocator, m_gpuSceneData.allocation, reinterpret_cast<void **>( &gpu_scene_addr ) );
    *gpu_scene_addr = sceneData;
    gpu_scene_addr->materials = gfx.materialCodex.GetDeviceAddress( );
    vmaUnmapMemory( gfx.allocator, m_gpuSceneData.allocation );

    vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline );

    auto bindless_set = gfx.GetBindlessSet( );
    vkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_layout, 0, 1, &bindless_set, 0, nullptr );

    m_gpuIbl.Upload( gfx, &m_ibl, sizeof( IblSettings ) );
    m_gpuDirectionalLights.Upload( gfx, directionalLights.data( ), sizeof( GpuDirectionalLight ) * directionalLights.size( ) );
    m_gpuPointLights.Upload( gfx, pointLights.data( ), sizeof( GpuPointLightData ) * pointLights.size( ) );

    DescriptorWriter writer;
    writer.WriteBuffer( 0, m_gpuIbl.buffer, sizeof( IblSettings ), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER );
    writer.WriteBuffer( 1, m_gpuDirectionalLights.buffer, sizeof( GpuDirectionalLight ) * 10, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER );
    writer.WriteBuffer( 2, m_gpuPointLights.buffer, sizeof( GpuPointLightData ) * 10, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER );
    auto set = m_sets.GetCurrentFrame( );
    writer.UpdateSet( gfx.device, set );
    vkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_layout, 1, 1, &set, 0, nullptr );

    auto &target_image = gfx.imageCodex.GetImage( gfx.swapchain.GetCurrentFrame( ).hdrColor );

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
            .buffer = m_gpuSceneData.buffer };
    auto gpu_scene_address = vkGetBufferDeviceAddress( gfx.device, &address_info );

    const PushConstants push_constants = {
            .sceneDataAddress = gpu_scene_address,
            .albedoTex = gBuffer.albedo,
            .normalTex = gBuffer.normal,
            .positionTex = gBuffer.position,
            .pbrTex = gBuffer.pbr,
            .irradianceTex = irradianceMap,
            .radianceTex = radianceMap,
            .brdfLut = brdfLut,
            .ssaoTex = gfx.swapchain.GetCurrentFrame( ).ssao };
    vkCmdPushConstants( cmd, m_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( PushConstants ), &push_constants );

    vkCmdDraw( cmd, 3, 1, 0, 0 );

    stats.drawcallCount++;
    stats.triangleCount += 1;

    return stats;
}

void PbrPipeline::DrawDebug( ) {
    if ( ImGui::CollapsingHeader( "IBL Settings" ) ) {
        ImGui::Indent( );
        ImGui::DragFloat( "Radiance", &m_ibl.radianceFactor, 0.01f, 0.0f );
        ImGui::DragFloat( "Irradiance", &m_ibl.irradianceFactor, 0.01f, 0.0f );
        ImGui::DragFloat( "BRDF", &m_ibl.brdfFactor, 0.01f, 0.0f, 1.0f );
        ImGui::Unindent( );
    }
}

void PbrPipeline::Reconstruct( GfxDevice &gfx ) {
    auto &frag_shader = gfx.shaderStorage->Get( "pbr", TFragment );
    auto &vert_shader = gfx.shaderStorage->Get( "fullscreen_tri", TVertex );

    VkPushConstantRange range = {
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset = 0,
            .size = sizeof( PushConstants ) };

    DescriptorLayoutBuilder layout_builder;
    layout_builder.AddBinding( 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER );
    layout_builder.AddBinding( 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER );
    layout_builder.AddBinding( 2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER );
    m_ubLayout = layout_builder.Build( gfx.device, VK_SHADER_STAGE_FRAGMENT_BIT );

    m_sets = gfx.AllocateMultiSet( m_ubLayout );

    auto bindless_layout = gfx.GetBindlessLayout( );

    VkDescriptorSetLayout layouts[] = { bindless_layout, m_ubLayout };

    // pipeline
    VkPipelineLayoutCreateInfo layout_info = PipelineLayoutCreateInfo( );
    layout_info.pSetLayouts = layouts;
    layout_info.setLayoutCount = 2;
    layout_info.pPushConstantRanges = &range;
    layout_info.pushConstantRangeCount = 1;
    VK_CHECK( vkCreatePipelineLayout( gfx.device, &layout_info, nullptr, &m_layout ) );

    PipelineBuilder builder;
    builder.SetShaders( vert_shader.handle, frag_shader.handle );
    builder.SetInputTopology( VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST );
    builder.SetPolygonMode( VK_POLYGON_MODE_FILL );
    builder.SetCullMode( VK_CULL_MODE_FRONT_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE );
    builder.SetMultisamplingNone( );
    builder.DisableBlending( );
    builder.DisableDepthTest( );

    auto &color = gfx.imageCodex.GetImage( gfx.swapchain.GetCurrentFrame( ).hdrColor );
    auto &depth = gfx.imageCodex.GetImage( gfx.swapchain.GetCurrentFrame( ).depth );
    builder.SetColorAttachmentFormat( color.GetFormat( ) );
    builder.SetDepthFormat( depth.GetFormat( ) );
    builder.SetLayout( m_layout );
    m_pipeline = builder.Build( gfx.device );

#ifdef ENABLE_DEBUG_UTILS
    const VkDebugUtilsObjectNameInfoEXT obj = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
            .pNext = nullptr,
            .objectType = VK_OBJECT_TYPE_PIPELINE,
            .objectHandle = reinterpret_cast<uint64_t>( m_pipeline ),
            .pObjectName = "PBR Pipeline" };
    vkSetDebugUtilsObjectNameEXT( gfx.device, &obj );
#endif

    m_gpuSceneData = gfx.Allocate( sizeof( GpuSceneData ), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                   VMA_MEMORY_USAGE_CPU_TO_GPU, "Scene Data PBR Pipeline" );

    m_gpuIbl = gfx.Allocate( sizeof( IblSettings ), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, "IBL Settings" );
    m_gpuDirectionalLights = gfx.Allocate( sizeof( GpuDirectionalLight ) * 10, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, "Directional Lights" );
    m_gpuPointLights = gfx.Allocate( sizeof( GpuPointLightData ) * 10, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, "Point Lights" );
}
