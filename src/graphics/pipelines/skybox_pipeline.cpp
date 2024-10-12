#include <pch.h>

#include "skybox_pipeline.h"

#include <vk_initializers.h>
#include <vk_pipelines.h>
#include "compute_pipeline.h"

using namespace vk_init;

SkyboxPipeline::Result<> SkyboxPipeline::Init( GfxDevice &gfx ) {
    auto &frag_shader = gfx.shaderStorage->Get( "skybox", TFragment );
    auto &vert_shader = gfx.shaderStorage->Get( "skybox", TVertex );

    auto reconstruct_shader_callback = [&]( VkShaderModule shader ) {
        VK_CHECK( vkWaitForFences( gfx.device, 1, &gfx.swapchain.GetCurrentFrame( ).fence, true, 1000000000 ) );
        Cleanup( gfx );

        Reconstruct( gfx );
    };

    auto _ = vert_shader.RegisterReloadCallback( reconstruct_shader_callback );
    _ = frag_shader.RegisterReloadCallback( reconstruct_shader_callback );

    Reconstruct( gfx );

    // create cube mesh
    CreateCubeMesh( gfx );

    return { };
}

void SkyboxPipeline::CreateCubeMesh( GfxDevice &gfx ) {
    const std::vector<Mesh::Vertex> vertices = {
            // Front face
            { { -1.0f, 1.0f, 1.0f }, 0.0f, { 0.0f, 0.0f, 1.0f }, 0.0f, { 1.0f, 2.0f, 3.0f }, 0.0f, { 4.0f, 5.0f, 6.0f }, 0.0f },
            { { 1.0f, 1.0f, 1.0f }, 1.0f, { 0.0f, 0.0f, 1.0f }, 0.0f, { 1.0f, 0.0f, 0.0f }, 0.0f, { 0.0f, 0.0f, 0.0f }, 0.0f },
            { { 1.0f, -1.0f, 1.0f }, 1.0f, { 0.0f, 0.0f, 1.0f }, 1.0f, { 1.0f, 0.0f, 0.0f }, 0.0f, { 0.0f, 0.0f, 0.0f }, 0.0f },
            { { -1.0f, -1.0f, 1.0f }, 0.0f, { 0.0f, 0.0f, 1.0f }, 1.0f, { 1.0f, 0.0f, 0.0f }, 0.0f, { 0.0f, 0.0f, 0.0f }, 0.0f },

            // Back face
            { { -1.0f, 1.0f, -1.0f }, 1.0f, { 0.0f, 0.0f, -1.0f }, 0.0f, { -1.0f, 0.0f, 0.0f }, 0.0f, { 0.0f, 0.0f, 0.0f }, 0.0f },
            { { 1.0f, 1.0f, -1.0f }, 0.0f, { 0.0f, 0.0f, -1.0f }, 0.0f, { -1.0f, 0.0f, 0.0f }, 0.0f, { 0.0f, 0.0f, 0.0f }, 0.0f },
            { { 1.0f, -1.0f, -1.0f }, 0.0f, { 0.0f, 0.0f, -1.0f }, 1.0f, { -1.0f, 0.0f, 0.0f }, 0.0f, { 0.0f, 0.0f, 0.0f }, 0.0f },
            { { -1.0f, -1.0f, -1.0f }, 1.0f, { 0.0f, 0.0f, -1.0f }, 1.0f, { -1.0f, 0.0f, 0.0f }, 0.0f, { 0.0f, 0.0f, 0.0f }, 0.0f } };

    const std::vector<uint32_t> indices = {
            // Front face
            0, 1, 2,
            2, 3, 0,

            // Right face
            1, 5, 6,
            6, 2, 1,

            // Back face
            5, 4, 7,
            7, 6, 5,

            // Left face
            4, 0, 3,
            3, 7, 4,

            // Top face
            4, 5, 1,
            1, 0, 4,

            // Bottom face
            3, 2, 6,
            6, 7, 3 };

    const Mesh mesh = {
            .vertices = vertices,
            .indices = indices,
    };

    m_cubeMesh = gfx.meshCodex.AddMesh( gfx, mesh );
}

void SkyboxPipeline::Reconstruct( GfxDevice &gfx ) {
    auto &frag_shader = gfx.shaderStorage->Get( "skybox", TFragment );
    auto &vert_shader = gfx.shaderStorage->Get( "skybox", TVertex );

    VkPushConstantRange range = {
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset = 0,
            .size = sizeof( PushConstants ),
    };

    auto bindless_layout = gfx.GetBindlessLayout( );
    VkDescriptorSetLayout layouts[] = { bindless_layout };

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
    builder.EnableDepthTest( false, VK_COMPARE_OP_LESS_OR_EQUAL );

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
            .pObjectName = "Skybox Pipeline" };
    vkSetDebugUtilsObjectNameEXT( gfx.device, &obj );
#endif

    m_gpuSceneData = gfx.Allocate( sizeof( GpuSceneData ), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, "Scene Data GBuffer Pipeline" );
}

void SkyboxPipeline::Cleanup( GfxDevice &gfx ) {
    vkDestroyPipelineLayout( gfx.device, m_layout, nullptr );
    vkDestroyPipeline( gfx.device, m_pipeline, nullptr );
    gfx.Free( m_gpuSceneData );
}

void SkyboxPipeline::Draw( GfxDevice &gfx, VkCommandBuffer cmd, ImageId skyboxTexture, const GpuSceneData &sceneData ) const {
    GpuSceneData *gpu_scene_addr = nullptr;
    vmaMapMemory( gfx.allocator, m_gpuSceneData.allocation, reinterpret_cast<void **>( &gpu_scene_addr ) );
    *gpu_scene_addr = sceneData;
    gpu_scene_addr->materials = gfx.materialCodex.GetDeviceAddress( );
    vmaUnmapMemory( gfx.allocator, m_gpuSceneData.allocation );

    vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline );

    const auto bindless_set = gfx.GetBindlessSet( );
    vkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_layout, 0, 1, &bindless_set, 0, nullptr );

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

    auto &mesh = gfx.meshCodex.GetMesh( m_cubeMesh );
    vkCmdBindIndexBuffer( cmd, mesh.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32 );

    const VkBufferDeviceAddressInfo address_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            .pNext = nullptr,
            .buffer = m_gpuSceneData.buffer,
    };
    const auto gpu_scene_address = vkGetBufferDeviceAddress( gfx.device, &address_info );

    const PushConstants push_constants = {
            .sceneDataAddress = gpu_scene_address,
            .vertexBufferAddress = mesh.vertexBufferAddress,
            .textureId = skyboxTexture,
    };
    vkCmdPushConstants( cmd, m_layout, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof( PushConstants ), &push_constants );

    vkCmdDrawIndexed( cmd, mesh.indexCount, 1, 0, 0, 0 );
}
