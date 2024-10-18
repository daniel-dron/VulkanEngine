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

#include "vk_engine.h"

#include <SDL.h>
#include <VkBootstrap.h>
#include <vk_initializers.h>
#include <vk_types.h>
#include <vulkan/vulkan_core.h>

#include <graphics/descriptors.h>
#include "SDL_events.h"
#include "SDL_stdinc.h"
#include "SDL_video.h"
#include "fmt/core.h"
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/gtx/integer.hpp"
#include "glm/gtx/matrix_decompose.hpp"
#include "glm/packing.hpp"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#define TRACY_ENABLE
#include <glm/gtc/type_ptr.hpp>

#include <graphics/pipelines/compute_pipeline.h>
#include "engine/input.h"
#include "imguizmo/ImGuizmo.h"
#include "tracy/TracyClient.cpp"
#include "tracy/tracy/Tracy.hpp"

#include <engine/loader.h>
#include <glm/gtx/euler_angles.hpp>

#include <utils/ImGuiProfilerRenderer.h>
#include "graphics/draw_command.h"

VulkanEngine *g_loadedEngine = nullptr;

// TODO: move
void GpuBuffer::Upload( const GfxDevice &gfx, const void *data, const size_t size ) const {
    void *mapped_buffer = { };

    vmaMapMemory( gfx.allocator, allocation, &mapped_buffer );
    memcpy( mapped_buffer, data, size );
    vmaUnmapMemory( gfx.allocator, allocation );
}

VkDeviceAddress GpuBuffer::GetDeviceAddress( const GfxDevice &gfx ) {
    if ( deviceAddress == 0 ) {
        const VkBufferDeviceAddressInfo address_info = {
                .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                .pNext = nullptr,
                .buffer = buffer,
        };

        deviceAddress = vkGetBufferDeviceAddress( gfx.device, &address_info );
    }

    return deviceAddress;
}

VulkanEngine &VulkanEngine::Get( ) { return *g_loadedEngine; }

void VulkanEngine::Init( ) {
    assert( g_loadedEngine == nullptr );
    g_loadedEngine = this;

    InitSdl( );
    InitVulkan( );
    InitDefaultData( );
    InitImGui( );
    m_imGuiPipeline.Init( *m_gfx );
    EG_INPUT.Init( );
    InitScene( );

    m_visualProfiler.RegisterTask( "Main", utils::colors::CARROT, utils::VisualProfiler::Cpu );
    m_visualProfiler.RegisterTask( "Create Commands", utils::colors::EMERALD, utils::VisualProfiler::Cpu );
    m_visualProfiler.RegisterTask( "Scene", utils::colors::EMERALD, utils::VisualProfiler::Cpu );

    m_visualProfiler.RegisterTask( "ShadowMap", utils::colors::TURQUOISE, utils::VisualProfiler::Gpu );
    m_visualProfiler.RegisterTask( "GBuffer", utils::colors::ALIZARIN, utils::VisualProfiler::Gpu );
    m_visualProfiler.RegisterTask( "SSAO", utils::colors::SILVER, utils::VisualProfiler::Gpu );
    m_visualProfiler.RegisterTask( "Lighting", utils::colors::AMETHYST, utils::VisualProfiler::Gpu );
    m_visualProfiler.RegisterTask( "Skybox", utils::colors::SUN_FLOWER, utils::VisualProfiler::Gpu );
    m_visualProfiler.RegisterTask( "Post Process", utils::colors::PETER_RIVER, utils::VisualProfiler::Gpu );

    m_isInitialized = true;
}

void VulkanEngine::InitSdl( ) {
    SDL_Init( SDL_INIT_VIDEO );

    constexpr auto window_flags = static_cast<SDL_WindowFlags>( SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE );

    m_window = SDL_CreateWindow( "Vulkan Engine", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                 m_windowExtent.width, m_windowExtent.height, window_flags );
}

void VulkanEngine::InitVulkan( ) {
    m_gfx = std::make_unique<GfxDevice>( );

    if ( m_rendererOptions.vsync ) {
        m_gfx->swapchain.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    }
    else {
        m_gfx->swapchain.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    }

    m_gfx->Init( m_window );

    m_mainDeletionQueue.Flush( );
}

void VulkanEngine::InitImGui( ) {
    const VkDescriptorPoolSize pool_sizes[] = {
            { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 },
    };

    const VkDescriptorPoolCreateInfo pool_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
            .maxSets = 1000,
            .poolSizeCount = static_cast<uint32_t>( std::size( pool_sizes ) ),
            .pPoolSizes = pool_sizes,
    };

    VkDescriptorPool imgui_pool;
    VK_CHECK( vkCreateDescriptorPool( m_gfx->device, &pool_info, nullptr, &imgui_pool ) );

    ImGui::CreateContext( );

    ImGui_ImplSDL2_InitForVulkan( m_window );

    ImGui_ImplVulkan_InitInfo init_info = {
            .Instance = m_gfx->instance,
            .PhysicalDevice = m_gfx->chosenGpu,
            .Device = m_gfx->device,
            .Queue = m_gfx->graphicsQueue,
            .DescriptorPool = imgui_pool,
            .MinImageCount = 3,
            .ImageCount = 3,
            .UseDynamicRendering = true,
    };

    init_info.PipelineRenderingCreateInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
    init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats = &m_gfx->swapchain.format;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init( &init_info );
    ImGui_ImplVulkan_CreateFontsTexture( );

    auto &io = ImGui::GetIO( );
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImGui::StyleColorsDark( );

    ImGuiStyle &style = ImGui::GetStyle( );
    ImVec4 *colors = style.Colors;

    colors[ImGuiCol_Text] = ImVec4( 0.80f, 0.80f, 0.80f, 1.00f );
    colors[ImGuiCol_TextDisabled] = ImVec4( 0.50f, 0.50f, 0.50f, 1.00f );
    colors[ImGuiCol_WindowBg] = ImVec4( 0.10f, 0.10f, 0.10f, 1.00f );
    colors[ImGuiCol_ChildBg] = ImVec4( 0.10f, 0.10f, 0.10f, 1.00f );
    colors[ImGuiCol_PopupBg] = ImVec4( 0.10f, 0.10f, 0.10f, 1.00f );
    colors[ImGuiCol_Border] = ImVec4( 0.40f, 0.40f, 0.40f, 1.00f );
    colors[ImGuiCol_BorderShadow] = ImVec4( 0.00f, 0.00f, 0.00f, 0.00f );
    colors[ImGuiCol_FrameBg] = ImVec4( 0.20f, 0.20f, 0.20f, 1.00f );
    colors[ImGuiCol_FrameBgHovered] = ImVec4( 0.30f, 0.30f, 0.30f, 1.00f );
    colors[ImGuiCol_FrameBgActive] = ImVec4( 0.40f, 0.40f, 0.40f, 1.00f );
    colors[ImGuiCol_TitleBg] = ImVec4( 0.30f, 0.30f, 0.30f, 1.00f );
    colors[ImGuiCol_TitleBgActive] = ImVec4( 0.40f, 0.40f, 0.40f, 1.00f );
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4( 0.00f, 0.00f, 0.00f, 0.51f );
    colors[ImGuiCol_MenuBarBg] = ImVec4( 0.14f, 0.14f, 0.14f, 1.00f );
    colors[ImGuiCol_ScrollbarBg] = ImVec4( 0.02f, 0.02f, 0.02f, 0.53f );
    colors[ImGuiCol_ScrollbarGrab] = ImVec4( 0.31f, 0.31f, 0.31f, 1.00f );
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4( 0.41f, 0.41f, 0.41f, 1.00f );
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4( 0.51f, 0.51f, 0.51f, 1.00f );
    colors[ImGuiCol_CheckMark] = ImVec4( 0.98f, 0.26f, 0.26f, 1.00f );
    colors[ImGuiCol_SliderGrab] = ImVec4( 0.88f, 0.24f, 0.24f, 1.00f );
    colors[ImGuiCol_SliderGrabActive] = ImVec4( 0.98f, 0.26f, 0.26f, 1.00f );
    colors[ImGuiCol_Button] = ImVec4( 0.30f, 0.30f, 0.30f, 1.00f );
    colors[ImGuiCol_ButtonHovered] = ImVec4( 0.40f, 0.40f, 0.40f, 1.00f );
    colors[ImGuiCol_ButtonActive] = ImVec4( 0.50f, 0.50f, 0.50f, 1.00f );
    colors[ImGuiCol_Header] = ImVec4( 0.30f, 0.30f, 0.30f, 1.00f );
    colors[ImGuiCol_HeaderHovered] = ImVec4( 0.40f, 0.40f, 0.40f, 1.00f );
    colors[ImGuiCol_HeaderActive] = ImVec4( 0.50f, 0.50f, 0.50f, 1.00f );
    colors[ImGuiCol_Separator] = ImVec4( 0.43f, 0.43f, 0.50f, 0.50f );
    colors[ImGuiCol_SeparatorHovered] = ImVec4( 0.75f, 0.10f, 0.10f, 0.78f );
    colors[ImGuiCol_SeparatorActive] = ImVec4( 0.75f, 0.10f, 0.10f, 1.00f );
    colors[ImGuiCol_ResizeGrip] = ImVec4( 0.98f, 0.26f, 0.26f, 0.20f );
    colors[ImGuiCol_ResizeGripHovered] = ImVec4( 0.98f, 0.26f, 0.26f, 0.67f );
    colors[ImGuiCol_ResizeGripActive] = ImVec4( 0.98f, 0.26f, 0.26f, 0.95f );
    colors[ImGuiCol_TabHovered] = ImVec4( 0.40f, 0.40f, 0.40f, 1.00f );
    colors[ImGuiCol_Tab] = ImVec4( 0.30f, 0.30f, 0.30f, 1.00f );
    colors[ImGuiCol_TabSelected] = ImVec4( 0.50f, 0.50f, 0.50f, 1.00f );
    colors[ImGuiCol_TabSelectedOverline] = ImVec4( 0.98f, 0.26f, 0.26f, 1.00f );
    colors[ImGuiCol_TabDimmed] = ImVec4( 0.07f, 0.10f, 0.15f, 0.97f );
    colors[ImGuiCol_TabDimmedSelected] = ImVec4( 0.42f, 0.14f, 0.14f, 1.00f );
    colors[ImGuiCol_TabDimmedSelectedOverline] = ImVec4( 0.50f, 0.50f, 0.50f, 1.00f );
    colors[ImGuiCol_DockingPreview] = ImVec4( 0.98f, 0.26f, 0.26f, 0.70f );
    colors[ImGuiCol_DockingEmptyBg] = ImVec4( 0.20f, 0.20f, 0.20f, 1.00f );
    colors[ImGuiCol_PlotLines] = ImVec4( 0.61f, 0.61f, 0.61f, 1.00f );
    colors[ImGuiCol_PlotLinesHovered] = ImVec4( 1.00f, 0.43f, 0.35f, 1.00f );
    colors[ImGuiCol_PlotHistogram] = ImVec4( 0.90f, 0.70f, 0.00f, 1.00f );
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4( 1.00f, 0.60f, 0.00f, 1.00f );
    colors[ImGuiCol_TableHeaderBg] = ImVec4( 0.19f, 0.19f, 0.20f, 1.00f );
    colors[ImGuiCol_TableBorderStrong] = ImVec4( 0.31f, 0.31f, 0.35f, 1.00f );
    colors[ImGuiCol_TableBorderLight] = ImVec4( 0.23f, 0.23f, 0.25f, 1.00f );
    colors[ImGuiCol_TableRowBg] = ImVec4( 0.00f, 0.00f, 0.00f, 0.00f );
    colors[ImGuiCol_TableRowBgAlt] = ImVec4( 1.00f, 1.00f, 1.00f, 0.06f );
    colors[ImGuiCol_TextLink] = ImVec4( 0.98f, 0.26f, 0.26f, 1.00f );
    colors[ImGuiCol_TextSelectedBg] = ImVec4( 0.98f, 0.26f, 0.26f, 0.35f );
    colors[ImGuiCol_DragDropTarget] = ImVec4( 1.00f, 1.00f, 0.00f, 0.90f );
    colors[ImGuiCol_NavHighlight] = ImVec4( 0.98f, 0.26f, 0.26f, 1.00f );
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4( 1.00f, 1.00f, 1.00f, 0.70f );
    colors[ImGuiCol_NavWindowingDimBg] = ImVec4( 0.80f, 0.80f, 0.80f, 0.20f );
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4( 0.80f, 0.80f, 0.80f, 0.35f );


    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.WindowRounding = 5.0f;
    style.ChildRounding = 5.0f;
    style.FrameRounding = 5.0f;
    style.PopupRounding = 5.0f;
    style.ScrollbarRounding = 5.0f;
    style.GrabRounding = 5.0f;
    style.TabRounding = 5.0f;
    style.WindowTitleAlign = ImVec2( 0.0f, 0.5f );
    style.ItemSpacing = ImVec2( 8, 4 );
    style.FramePadding = ImVec2( 4, 2 );

    m_mainDeletionQueue.PushFunction( [&, imgui_pool]( ) {
        ImGui_ImplVulkan_Shutdown( );
        vkDestroyDescriptorPool( m_gfx->device, imgui_pool, nullptr );
    } );
}

auto RandomRange( const float min, const float max ) -> float {
    static std::random_device rd;
    static std::mt19937 gen( rd( ) );

    std::uniform_real_distribution<float> dis( min, max );

    return dis( gen );
}

float Lerp( const float a, const float b, const float f ) { return a + f * ( b - a ); }

void VulkanEngine::ConstructSsaoPipeline( ) {
    m_ssaoBuffer = m_gfx->Allocate( sizeof( SsaoSettings ), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                    VMA_MEMORY_USAGE_CPU_TO_GPU, "SSAO Settings" );
    m_ssaoKernel = m_gfx->Allocate( sizeof( Vec3 ) * 64, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                    VMA_MEMORY_USAGE_CPU_TO_GPU, "SSAO Kernel" );

    std::vector<glm::vec3> kernels;
    for ( int i = 0; i < m_ssaoSettings.kernelSize; i++ ) {
        glm::vec3 sample( RandomRange( 0.0, 1.0 ) * 2.0 - 1.0, RandomRange( 0.0, 1.0 ) * 2.0 - 1.0,
                          RandomRange( 0.0, 1.0 ) );
        sample = glm::normalize( sample );
        sample *= RandomRange( 0.0, 1.0 );

        const float scale = static_cast<float>( i ) / static_cast<float>( m_ssaoSettings.kernelSize );
        const float scale_mul = Lerp( 0.1f, 1.0f, scale * scale );
        sample *= scale_mul;

        kernels.push_back( sample );
    }
    m_ssaoKernel.Upload( *m_gfx, kernels.data( ), kernels.size( ) * sizeof( Vec3 ) );

    std::vector<glm::vec4> noise_data;
    for ( int i = 0; i < 16; i++ ) {
        glm::vec3 noise( RandomRange( 0.0, 1.0 ), RandomRange( 0.0, 1.0 ), 0.0f );
        noise_data.push_back( glm::vec4( noise, 1.0f ) );
    }
    m_ssaoSettings.noiseTexture =
            m_gfx->imageCodex.LoadImageFromData( "SSAO Noise", noise_data.data( ), VkExtent3D{ 4, 4, 1 },
                                                 VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT, false );

    m_ssaoSettings.depthTexture = m_gfx->swapchain.GetCurrentFrame( ).depth;
    m_ssaoSettings.normalTexture = m_gfx->swapchain.GetCurrentFrame( ).gBuffer.normal;
    m_ssaoSettings.scene = m_gpuSceneData.GetDeviceAddress( *m_gfx );
    m_ssaoBuffer.Upload( *m_gfx, &m_ssaoSettings, sizeof( SsaoSettings ) );

    // Create pipeline
    auto &shader = m_gfx->shaderStorage->Get( "ssao", TCompute );
    shader.RegisterReloadCallback( [&]( VkShaderModule module ) {
        VK_CHECK( vkWaitForFences( m_gfx->device, 1, &m_gfx->swapchain.GetCurrentFrame( ).fence, true, 1000000000 ) );
        m_ssaoPipeline.Cleanup( *m_gfx );

        ActuallyConstructSsaoPipeline( );
    } );

    ActuallyConstructSsaoPipeline( );
}

void VulkanEngine::ActuallyConstructSsaoPipeline( ) {
    auto &shader = m_gfx->shaderStorage->Get( "ssao", TCompute );
    m_ssaoPipeline.AddDescriptorSetLayout( 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER );
    m_ssaoPipeline.AddDescriptorSetLayout( 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER );
    m_ssaoPipeline.AddDescriptorSetLayout( 2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE );
    m_ssaoPipeline.Build( *m_gfx, shader.handle, "ssao compute" );

    m_ssaoSet = m_gfx->AllocateMultiSet( m_ssaoPipeline.GetLayout( ) );

    DescriptorWriter writer;
    writer.WriteBuffer( 0, m_ssaoBuffer.buffer, sizeof( SsaoSettings ), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER );
    writer.WriteBuffer( 1, m_ssaoKernel.buffer, sizeof( Vec3 ) * 64, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER );

    for ( auto i = 0; i < m_gfx->swapchain.FrameOverlap; i++ ) {
        auto &ssao_image = m_gfx->imageCodex.GetImage( m_gfx->swapchain.frames[i].ssao );
        writer.WriteImage( 2, ssao_image.GetBaseView( ), nullptr, VK_IMAGE_LAYOUT_GENERAL,
                           VK_DESCRIPTOR_TYPE_STORAGE_IMAGE );
        writer.UpdateSet( m_gfx->device, m_ssaoSet.m_sets[i] );
    }
}

void VulkanEngine::ConstructBlurPipeline( ) {
    auto &shader = m_gfx->shaderStorage->Get( "blur", TCompute );
    shader.RegisterReloadCallback( [&]( VkShaderModule module ) {
        VK_CHECK( vkWaitForFences( m_gfx->device, 1, &m_gfx->swapchain.GetCurrentFrame( ).fence, true, 1000000000 ) );
        m_blurPipeline.Cleanup( *m_gfx );

        ActuallyConstructBlurPipeline( );
    } );

    ActuallyConstructBlurPipeline( );
}

void VulkanEngine::ActuallyConstructBlurPipeline( ) {
    auto &shader = m_gfx->shaderStorage->Get( "blur", TCompute );
    m_blurPipeline.AddDescriptorSetLayout( 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE );
    m_blurPipeline.AddPushConstantRange( sizeof( BlurSettings ) );
    m_blurPipeline.Build( *m_gfx, shader.handle, "blur pipeline" );

    m_blurSet = m_gfx->AllocateMultiSet( m_blurPipeline.GetLayout( ) );
}

void VulkanEngine::ResizeSwapchain( uint32_t width, uint32_t height ) {
    vkDeviceWaitIdle( m_gfx->device );

    m_windowExtent.width = width;
    m_windowExtent.height = height;

    m_gfx->swapchain.Recreate( width, height );
}

void VulkanEngine::Cleanup( ) {
    if ( m_isInitialized ) {
        // wait for gpu work to finish
        vkDeviceWaitIdle( m_gfx->device );

        m_pbrPipeline.Cleanup( *m_gfx );
        m_wireframePipeline.Cleanup( *m_gfx );
        m_gBufferPipeline.Cleanup( *m_gfx );
        m_imGuiPipeline.Cleanup( *m_gfx );
        m_skyboxPipeline.Cleanup( *m_gfx );
        m_shadowMapPipeline.Cleanup( *m_gfx );
        m_blurPipeline.Cleanup( *m_gfx );

        m_postProcessPipeline.Cleanup( *m_gfx );
        m_ssaoPipeline.Cleanup( *m_gfx );
        m_gfx->Free( m_ssaoBuffer );
        m_gfx->Free( m_ssaoKernel );

        m_ibl.Clean( *m_gfx );

        m_mainDeletionQueue.Flush( );

        m_gfx->Cleanup( );

        SDL_DestroyWindow( m_window );
    }

    // clear engine pointer
    g_loadedEngine = nullptr;
}

void VulkanEngine::Draw( ) {
    ZoneScopedN( "draw" );

    // wait for last frame rendering phase. 1 sec timeout
    VK_CHECK( vkWaitForFences( m_gfx->device, 1, &m_gfx->swapchain.GetCurrentFrame( ).fence, true, 1000000000 ) );
    m_gfx->swapchain.GetCurrentFrame( ).deletionQueue.Flush( );
    // Reset after acquire the next image from the swapchain
    // In case of error, this fence would never get passed to the queue, thus
    // never triggering leaving us with a timeout next time we wait for fence
    VK_CHECK( vkResetFences( m_gfx->device, 1, &m_gfx->swapchain.GetCurrentFrame( ).fence ) );

    // Query the pool timings
    if ( m_gfx->swapchain.frameNumber != 0 ) {
        vkGetQueryPoolResults( m_gfx->device, m_gfx->queryPoolTimestamps, 0, m_gfx->gpuTimestamps.size( ),
                               m_gfx->gpuTimestamps.size( ) * sizeof( uint64_t ), m_gfx->gpuTimestamps.data( ),
                               sizeof( uint64_t ), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT );

        auto time = m_gfx->GetTimestampInMs( m_gfx->gpuTimestamps.at( 0 ), m_gfx->gpuTimestamps.at( 1 ) ) / 1000.0f;
        m_visualProfiler.AddTimer( "ShadowMap", time, utils::VisualProfiler::Gpu );
        time = m_gfx->GetTimestampInMs( m_gfx->gpuTimestamps.at( 2 ), m_gfx->gpuTimestamps.at( 3 ) ) / 1000.0f;
        m_visualProfiler.AddTimer( "GBuffer", time, utils::VisualProfiler::Gpu );
        time = m_gfx->GetTimestampInMs( m_gfx->gpuTimestamps.at( 4 ), m_gfx->gpuTimestamps.at( 5 ) ) / 1000.0f;
        m_visualProfiler.AddTimer( "SSAO", time, utils::VisualProfiler::Gpu );
        time = m_gfx->GetTimestampInMs( m_gfx->gpuTimestamps.at( 6 ), m_gfx->gpuTimestamps.at( 7 ) ) / 1000.0f;
        m_visualProfiler.AddTimer( "Lighting", time, utils::VisualProfiler::Gpu );
        time = m_gfx->GetTimestampInMs( m_gfx->gpuTimestamps.at( 8 ), m_gfx->gpuTimestamps.at( 9 ) ) / 1000.0f;
        m_visualProfiler.AddTimer( "Skybox", time, utils::VisualProfiler::Gpu );
        time = m_gfx->GetTimestampInMs( m_gfx->gpuTimestamps.at( 10 ), m_gfx->gpuTimestamps.at( 11 ) ) / 1000.0f;
        m_visualProfiler.AddTimer( "Post Process", time, utils::VisualProfiler::Gpu );
    }

    uint32_t swapchain_image_index;
    {
        ZoneScopedN( "vsync" );

        VkResult e = ( vkAcquireNextImageKHR( m_gfx->device, m_gfx->swapchain.swapchain, 1000000000,
                                              m_gfx->swapchain.GetCurrentFrame( ).swapchainSemaphore, nullptr,
                                              &swapchain_image_index ) );
        if ( e != VK_SUCCESS ) {
            return;
        }
    }

    auto &color = m_gfx->imageCodex.GetImage( m_gfx->swapchain.GetCurrentFrame( ).hdrColor );

    auto &depth = m_gfx->imageCodex.GetImage( m_gfx->swapchain.GetCurrentFrame( ).depth );

    m_drawExtent.height = static_cast<uint32_t>( std::min( m_gfx->swapchain.extent.height, color.GetExtent( ).height ) *
                                                 m_renderScale );
    m_drawExtent.width = static_cast<uint32_t>( std::min( m_gfx->swapchain.extent.width, color.GetExtent( ).width ) *
                                                m_renderScale );

    // commands
    const auto cmd = m_gfx->swapchain.GetCurrentFrame( ).commandBuffer;
    VK_CHECK( vkResetCommandBuffer( cmd, 0 ) );
    const auto cmd_begin_info = vk_init::CommandBufferBeginInfo( VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT );
    VK_CHECK( vkBeginCommandBuffer( cmd, &cmd_begin_info ) );
    vkCmdResetQueryPool( cmd, m_gfx->queryPoolTimestamps, 0, m_gfx->gpuTimestamps.size( ) );

    depth.TransitionLayout( cmd, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, true );

    vkCmdWriteTimestamp( cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, m_gfx->queryPoolTimestamps, 0 );
    if ( m_gfx->swapchain.frameNumber == 0 || m_rendererOptions.reRenderShadowMaps ) {
        m_rendererOptions.reRenderShadowMaps = false;
        ShadowMapPass( cmd );
    }
    vkCmdWriteTimestamp( cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_gfx->queryPoolTimestamps, 1 );

    vkCmdWriteTimestamp( cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, m_gfx->queryPoolTimestamps, 2 );
    GBufferPass( cmd );
    vkCmdWriteTimestamp( cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_gfx->queryPoolTimestamps, 3 );

    vkCmdWriteTimestamp( cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, m_gfx->queryPoolTimestamps, 4 );
    SsaoPass( cmd );
    vkCmdWriteTimestamp( cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_gfx->queryPoolTimestamps, 5 );

    vkCmdWriteTimestamp( cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, m_gfx->queryPoolTimestamps, 6 );
    PbrPass( cmd );
    vkCmdWriteTimestamp( cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_gfx->queryPoolTimestamps, 7 );

    vkCmdWriteTimestamp( cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, m_gfx->queryPoolTimestamps, 8 );
    SkyboxPass( cmd );
    vkCmdWriteTimestamp( cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_gfx->queryPoolTimestamps, 9 );

    vkCmdWriteTimestamp( cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, m_gfx->queryPoolTimestamps, 10 );
    PostProcessPass( cmd );
    vkCmdWriteTimestamp( cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_gfx->queryPoolTimestamps, 11 );


    if ( m_drawEditor ) {
        DrawImGui( cmd, m_gfx->swapchain.views[swapchain_image_index] );
        image::TransitionLayout( cmd, m_gfx->swapchain.images[swapchain_image_index], VK_IMAGE_LAYOUT_UNDEFINED,
                                 VK_IMAGE_LAYOUT_PRESENT_SRC_KHR );
    }
    else {
        auto &ppi = m_gfx->imageCodex.GetImage( m_gfx->swapchain.GetCurrentFrame( ).postProcessImage );
        ppi.TransitionLayout( cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL );

        image::TransitionLayout( cmd, m_gfx->swapchain.images[swapchain_image_index], VK_IMAGE_LAYOUT_UNDEFINED,
                                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL );

        image::Blit( cmd, ppi.GetImage( ), { ppi.GetExtent( ).width, ppi.GetExtent( ).height },
                     m_gfx->swapchain.images[swapchain_image_index], m_gfx->swapchain.extent );
        if ( m_drawStats ) {
            DrawImGui( cmd, m_gfx->swapchain.views[swapchain_image_index] );
        }

        image::TransitionLayout( cmd, m_gfx->swapchain.images[swapchain_image_index],
                                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR );
    }

    VK_CHECK( vkEndCommandBuffer( cmd ) );

    //
    // send commands
    //

    // wait on _swapchainSemaphore. signaled when the swap chain is ready
    // wait on _renderSemaphore. signaled when rendering has finished
    auto cmdinfo = vk_init::CommandBufferSubmitInfo( cmd );

    auto waitInfo = vk_init::SemaphoreSubmitInfo( VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
                                                  m_gfx->swapchain.GetCurrentFrame( ).swapchainSemaphore );
    auto signalInfo = vk_init::SemaphoreSubmitInfo( VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
                                                    m_gfx->swapchain.GetCurrentFrame( ).renderSemaphore );

    auto submit = vk_init::SubmitInfo( &cmdinfo, &signalInfo, &waitInfo );

    // submit command buffer and execute it
    // _renderFence will now block until the commands finish
    VK_CHECK( vkQueueSubmit2( m_gfx->graphicsQueue, 1, &submit, m_gfx->swapchain.GetCurrentFrame( ).fence ) );

    //
    // present
    //
    VkPresentInfoKHR presentInfo = { };
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;
    presentInfo.pSwapchains = &m_gfx->swapchain.swapchain;
    presentInfo.swapchainCount = 1;

    // wait on _renderSemaphore, since we need the rendering to have finished
    // to display to the screen
    presentInfo.pWaitSemaphores = &m_gfx->swapchain.GetCurrentFrame( ).renderSemaphore;
    presentInfo.waitSemaphoreCount = 1;

    presentInfo.pImageIndices = &swapchain_image_index;

    vkQueuePresentKHR( m_gfx->graphicsQueue, &presentInfo );

    // increase frame number for next loop
    m_gfx->swapchain.frameNumber++;
}

void VulkanEngine::GBufferPass( VkCommandBuffer cmd ) const {
    ZoneScopedN( "GBuffer Pass" );
    START_LABEL( cmd, "GBuffer Pass", Vec4( 1.0f, 1.0f, 0.0f, 1.0 ) );

    using namespace vk_init;

    // ----------
    // Attachments
    {
        auto &gbuffer = m_gfx->swapchain.GetCurrentFrame( ).gBuffer;
        auto &albedo = m_gfx->imageCodex.GetImage( gbuffer.albedo );
        auto &normal = m_gfx->imageCodex.GetImage( gbuffer.normal );
        auto &position = m_gfx->imageCodex.GetImage( gbuffer.position );
        auto &pbr = m_gfx->imageCodex.GetImage( gbuffer.pbr );
        auto &depth = m_gfx->imageCodex.GetImage( m_gfx->swapchain.GetCurrentFrame( ).depth );

        VkClearValue clear_color = { 0.0f, 0.0f, 0.0f, 1.0f };
        std::array<VkRenderingAttachmentInfo, 4> color_attachments = {
                AttachmentInfo( albedo.GetBaseView( ), &clear_color ),
                AttachmentInfo( normal.GetBaseView( ), &clear_color ),
                AttachmentInfo( position.GetBaseView( ), &clear_color ),
                AttachmentInfo( pbr.GetBaseView( ), &clear_color ),
        };
        VkRenderingAttachmentInfo depth_attachment =
                DepthAttachmentInfo( depth.GetBaseView( ), VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL );

        VkRenderingInfo render_info = { .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                                        .pNext = nullptr,
                                        .renderArea = VkRect2D{ VkOffset2D{ 0, 0 }, m_drawExtent },
                                        .layerCount = 1,
                                        .colorAttachmentCount = color_attachments.size( ),
                                        .pColorAttachments = color_attachments.data( ),
                                        .pDepthAttachment = &depth_attachment,
                                        .pStencilAttachment = nullptr };
        vkCmdBeginRendering( cmd, &render_info );
    }

    // ----------
    // Call pipeline
    m_gBufferPipeline.Draw( *m_gfx, cmd, m_drawCommands, m_sceneData );

    vkCmdEndRendering( cmd );

    END_LABEL( cmd );
}

void VulkanEngine::SsaoPass( VkCommandBuffer cmd ) const {
    ZoneScopedN( "SSAO Pass" );
    START_LABEL( cmd, "SSAO Pass", Vec4( 1.0f, 0.5f, 0.3f, 1.0f ) );

    auto bindless = m_gfx->GetBindlessSet( );
    auto &output = m_gfx->imageCodex.GetImage( m_gfx->swapchain.GetCurrentFrame( ).ssao );

    output.TransitionLayout( cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL );

    m_ssaoPipeline.Bind( cmd );
    m_ssaoPipeline.BindDescriptorSet( cmd, bindless, 0 );
    m_ssaoPipeline.BindDescriptorSet( cmd, m_ssaoSet.GetCurrentFrame( ), 1 );
    m_ssaoPipeline.Dispatch( cmd, ( output.GetExtent( ).width + 15 ) / 16, ( output.GetExtent( ).height + 15 ) / 16,
                             6 );


    // ----------
    // Blur
    DescriptorWriter writer;
    writer.WriteImage( 0, output.GetBaseView( ), nullptr, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE );
    writer.UpdateSet( m_gfx->device, m_blurSet.GetCurrentFrame( ) );

    m_blurSettings.sourceTex = output.GetId( );
    m_blurSettings.size = 2;

    m_blurPipeline.Bind( cmd );
    m_blurPipeline.BindDescriptorSet( cmd, bindless, 0 );
    m_blurPipeline.BindDescriptorSet( cmd, m_blurSet.GetCurrentFrame( ), 1 );
    m_blurPipeline.PushConstants( cmd, sizeof( BlurSettings ), &m_blurSettings );
    m_blurPipeline.Dispatch( cmd, ( output.GetExtent( ).width + 15 ) / 16, ( output.GetExtent( ).height + 15 ) / 16,
                             6 );
    output.TransitionLayout( cmd, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );

    END_LABEL( cmd );
}

void VulkanEngine::PbrPass( VkCommandBuffer cmd ) const {
    ZoneScopedN( "PBR Pass" );
    START_LABEL( cmd, "PBR Pass", Vec4( 1.0f, 0.0f, 1.0f, 1.0f ) );

    using namespace vk_init;

    {
        auto &image = m_gfx->imageCodex.GetImage( m_gfx->swapchain.GetCurrentFrame( ).hdrColor );

        VkClearValue clear_color = { 0.0f, 0.0f, 0.0f, 0.0f };
        VkRenderingAttachmentInfo color_attachment = AttachmentInfo( image.GetBaseView( ), &clear_color );

        VkRenderingInfo render_info = { .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                                        .pNext = nullptr,
                                        .renderArea = VkRect2D{ VkOffset2D{ 0, 0 }, m_drawExtent },
                                        .layerCount = 1,
                                        .colorAttachmentCount = 1,
                                        .pColorAttachments = &color_attachment,
                                        .pDepthAttachment = nullptr,
                                        .pStencilAttachment = nullptr };
        vkCmdBeginRendering( cmd, &render_info );
    }

    m_pbrPipeline.Draw( *m_gfx, cmd, m_sceneData, m_gpuDirectionalLights, m_gpuPointLights,
                        m_gfx->swapchain.GetCurrentFrame( ).gBuffer, m_ibl.GetIrradiance( ), m_ibl.GetRadiance( ),
                        m_ibl.GetBrdf( ) );

    vkCmdEndRendering( cmd );

    END_LABEL( cmd );
}

void VulkanEngine::SkyboxPass( VkCommandBuffer cmd ) const {
    ZoneScopedN( "Skybox Pass" );
    START_LABEL( cmd, "Skybox Pass", Vec4( 0.0f, 1.0f, 0.0f, 1.0f ) );

    using namespace vk_init;

    {
        auto &image = m_gfx->imageCodex.GetImage( m_gfx->swapchain.GetCurrentFrame( ).hdrColor );
        auto &depth = m_gfx->imageCodex.GetImage( m_gfx->swapchain.GetCurrentFrame( ).depth );

        VkRenderingAttachmentInfo color_attachment = AttachmentInfo( image.GetBaseView( ), nullptr );

        VkRenderingAttachmentInfo depth_attachment = {
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .pNext = nullptr,
                .imageView = depth.GetBaseView( ),
                .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        };
        VkRenderingInfo render_info = { .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                                        .pNext = nullptr,
                                        .renderArea = VkRect2D{ VkOffset2D{ 0, 0 }, m_drawExtent },
                                        .layerCount = 1,
                                        .colorAttachmentCount = 1,
                                        .pColorAttachments = &color_attachment,
                                        .pDepthAttachment = &depth_attachment,
                                        .pStencilAttachment = nullptr };
        vkCmdBeginRendering( cmd, &render_info );
    }

    if ( m_rendererOptions.renderIrradianceInsteadSkybox ) {
        m_skyboxPipeline.Draw( *m_gfx, cmd, m_ibl.GetIrradiance( ), m_sceneData );
    }
    else {
        m_skyboxPipeline.Draw( *m_gfx, cmd, m_ibl.GetSkybox( ), m_sceneData );
    }

    vkCmdEndRendering( cmd );

    END_LABEL( cmd );
}

void VulkanEngine::PostProcessPass( VkCommandBuffer cmd ) const {
    auto bindless = m_gfx->GetBindlessSet( );
    auto &output = m_gfx->imageCodex.GetImage( m_gfx->swapchain.GetCurrentFrame( ).postProcessImage );

    m_ppConfig.hdr = m_gfx->swapchain.GetCurrentFrame( ).hdrColor;

    output.TransitionLayout( cmd, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL );

    m_postProcessPipeline.Bind( cmd );
    m_postProcessPipeline.BindDescriptorSet( cmd, bindless, 0 );
    m_postProcessPipeline.BindDescriptorSet( cmd, m_postProcessSet.GetCurrentFrame( ), 1 );
    m_postProcessPipeline.PushConstants( cmd, sizeof( PostProcessConfig ), &m_ppConfig );
    m_postProcessPipeline.Dispatch( cmd, ( output.GetExtent( ).width + 15 ) / 16,
                                    ( output.GetExtent( ).height + 15 ) / 16, 6 );

    output.TransitionLayout( cmd, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
}

void VulkanEngine::ShadowMapPass( VkCommandBuffer cmd ) const {
    ZoneScopedN( "ShadowMap Pass" );
    START_LABEL( cmd, "ShadowMap Pass", Vec4( 0.0f, 1.0f, 0.0f, 1.0f ) );

    using namespace vk_init;

    m_shadowMapPipeline.Draw( *m_gfx, cmd, m_shadowMapCommands, m_gpuDirectionalLights );

    END_LABEL( cmd );
}

void VulkanEngine::InitDefaultData( ) {
    InitImages( );

    m_gpuSceneData = m_gfx->Allocate( sizeof( GpuSceneData ),
                                      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                      VMA_MEMORY_USAGE_CPU_TO_GPU, "drawGeometry" );

    m_mainDeletionQueue.PushFunction( [=, this]( ) { m_gfx->Free( m_gpuSceneData ); } );

    m_pbrPipeline.Init( *m_gfx );
    m_wireframePipeline.Init( *m_gfx );
    m_gBufferPipeline.Init( *m_gfx );
    m_skyboxPipeline.Init( *m_gfx );
    m_shadowMapPipeline.Init( *m_gfx );

    // post process pipeline
    auto &post_process_shader = m_gfx->shaderStorage->Get( "post_process", TCompute );
    post_process_shader.RegisterReloadCallback( [&]( VkShaderModule shader ) {
        VK_CHECK( vkWaitForFences( m_gfx->device, 1, &m_gfx->swapchain.GetCurrentFrame( ).fence, true, 1000000000 ) );
        m_postProcessPipeline.Cleanup( *m_gfx );

        m_postProcessPipeline.AddDescriptorSetLayout( 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE );
        m_postProcessPipeline.AddPushConstantRange( sizeof( PostProcessConfig ) );
        m_postProcessPipeline.Build( *m_gfx, post_process_shader.handle, "post process compute" );
        m_postProcessSet = m_gfx->AllocateMultiSet( m_postProcessPipeline.GetLayout( ) );

        // TODO: this every frame for more than one inflight frame
        for ( auto i = 0; i < Swapchain::FrameOverlap; i++ ) {
            DescriptorWriter writer;
            auto &out_image = m_gfx->imageCodex.GetImage( m_gfx->swapchain.GetCurrentFrame( ).postProcessImage );
            writer.WriteImage( 0, out_image.GetBaseView( ), nullptr, VK_IMAGE_LAYOUT_GENERAL,
                               VK_DESCRIPTOR_TYPE_STORAGE_IMAGE );
            writer.UpdateSet( m_gfx->device, m_postProcessSet.m_sets[i] );
        }
    } );

    m_postProcessPipeline.AddDescriptorSetLayout( 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE );
    m_postProcessPipeline.AddPushConstantRange( sizeof( PostProcessConfig ) );
    m_postProcessPipeline.Build( *m_gfx, post_process_shader.handle, "post process compute" );
    m_postProcessSet = m_gfx->AllocateMultiSet( m_postProcessPipeline.GetLayout( ) );

    // TODO: this every frame for more than one inflight frame
    for ( auto i = 0; i < Swapchain::FrameOverlap; i++ ) {
        DescriptorWriter writer;
        auto &out_image = m_gfx->imageCodex.GetImage( m_gfx->swapchain.GetCurrentFrame( ).postProcessImage );
        writer.WriteImage( 0, out_image.GetBaseView( ), nullptr, VK_IMAGE_LAYOUT_GENERAL,
                           VK_DESCRIPTOR_TYPE_STORAGE_IMAGE );
        writer.UpdateSet( m_gfx->device, m_postProcessSet.m_sets[i] );
    }

    // SSAO pipeline
    ConstructSsaoPipeline( );
    ConstructBlurPipeline( );

    m_rendererOptions.ssaoResolution = { m_gfx->swapchain.extent.width, m_gfx->swapchain.extent.height };
}

void VulkanEngine::InitImages( ) {
    // 3 default textures, white, grey, black. 1 pixel each
    uint32_t white = glm::packUnorm4x8( glm::vec4( 1, 1, 1, 1 ) );
    m_whiteImage = m_gfx->imageCodex.LoadImageFromData( "debug_white_img", ( void * )&white, VkExtent3D{ 1, 1, 1 },
                                                        VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, false );

    uint32_t grey = glm::packUnorm4x8( glm::vec4( 0.66f, 0.66f, 0.66f, 1 ) );
    m_greyImage = m_gfx->imageCodex.LoadImageFromData( "debug_grey_img", ( void * )&grey, VkExtent3D{ 1, 1, 1 },
                                                       VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, false );

    uint32_t black = glm::packUnorm4x8( glm::vec4( 0, 0, 0, 0 ) );
    m_blackImage = m_gfx->imageCodex.LoadImageFromData( "debug_black_img", ( void * )&white, VkExtent3D{ 1, 1, 1 },
                                                        VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, false );

    // checkerboard image
    uint32_t magenta = glm::packUnorm4x8( glm::vec4( 1, 0, 1, 1 ) );
    std::array<uint32_t, 16 * 16> pixels; // for 16x16 checkerboard texture
    for ( int x = 0; x < 16; x++ ) {
        for ( int y = 0; y < 16; y++ ) {
            pixels[y * 16 + x] = ( ( x % 2 ) ^ ( y % 2 ) ) ? magenta : black;
        }
    }
    m_errorCheckerboardImage =
            m_gfx->imageCodex.LoadImageFromData( "debug_checkboard_img", ( void * )&white, VkExtent3D{ 16, 16, 1 },
                                                 VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, false );
}

void VulkanEngine::InitScene( ) {
    m_ibl.Init( *m_gfx, "../../assets/texture/ibls/belfast_sunset_4k.hdr" );

    m_scene = GltfLoader::Load( *m_gfx, "../../assets/untitled.glb" );

    // init camera
    if ( m_scene->cameras.empty( ) ) {
        m_camera = std::make_unique<Camera>( Vec3{ 0.225f, 0.138f, -0.920 }, 6.5f, 32.0f, WIDTH, HEIGHT );
    }
    else {
        auto &c = m_scene->cameras[0];
        m_camera = std::make_unique<Camera>( c );
        m_camera->SetAspectRatio( WIDTH, HEIGHT );
    }

    m_fpsController = std::make_unique<FirstPersonFlyingController>( m_camera.get( ), 0.1f, 5.0f );
    m_cameraController = m_fpsController.get( );
}

void VulkanEngine::DrawNodeHierarchy( const std::shared_ptr<Node> &node ) {
    if ( !node )
        return;

    std::string label = node->name.empty( ) ? "Unnamed Node" : node->name;
    label += "##" + std::to_string( reinterpret_cast<uintptr_t>( node.get( ) ) );

    if ( !node->children.empty( ) ) {
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
        if ( m_selectedNode == node ) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }
        const bool node_open = ImGui::TreeNodeEx( label.c_str( ), flags );

        if ( ImGui::IsItemClicked( ) ) {
            m_selectedNode = node;
        }

        if ( node == m_selectedNode ) {
            ImGui::SetItemDefaultFocus( );
        }

        if ( node_open ) {
            for ( const auto &child : node->children ) {
                DrawNodeHierarchy( child );
            }
            ImGui::TreePop( );
        }
    }
    else {
        if ( ImGui::Selectable( label.c_str( ), m_selectedNode == node ) ) {
            m_selectedNode = node;
            ImGui::SetItemDefaultFocus( );
        }
    }
}

static void drawSceneHierarchy( Node &node ) {
    node.transform.DrawDebug( node.name );

    for ( auto &n : node.children ) {
        drawSceneHierarchy( *n.get( ) );
    }
}

void VulkanEngine::Run( ) {
    bool b_quit = false;

    static ImageId selected_set = m_gfx->swapchain.GetCurrentFrame( ).postProcessImage;
    static int selected_set_n = 0;

    // main loop
    while ( !b_quit ) {
        FrameMarkNamed( "main" );
        auto start_task = utils::GetTime( );
        // begin clock
        auto start = std::chrono::system_clock::now( );

        {
            ZoneScopedN( "poll_events" );
            EG_INPUT.PollEvents( this );
        }

        if ( EG_INPUT.ShouldQuit( ) ) {
            b_quit = true;
        }

        if ( EG_INPUT.WasKeyPressed( EG_KEY::ESCAPE ) ) {
            b_quit = true;
        }

        if ( EG_INPUT.WasKeyPressed( EG_KEY::BACKSPACE ) ) {
            if ( m_drawEditor ) {
                m_drawEditor = false;
                m_drawStats = true;

                m_backupGamma = m_ppConfig.gamma;
                m_ppConfig.gamma = 1.0f;
            }
            else if ( m_drawStats == true && m_drawEditor == false ) {
                m_drawStats = false;
            }
            else {
                m_drawEditor = true;
                m_drawStats = true;

                m_ppConfig.gamma = m_backupGamma;
            }
        }

        static int saved_mouse_x, saved_mouse_y;
        // hide mouse
        if ( EG_INPUT.WasKeyPressed( EG_KEY::MOUSE_RIGHT ) ) {
            SDL_GetMouseState( &saved_mouse_x, &saved_mouse_y );
            SDL_SetRelativeMouseMode( SDL_TRUE );
            SDL_ShowCursor( SDL_DISABLE );
        }
        // reveal mouse
        if ( EG_INPUT.WasKeyReleased( EG_KEY::MOUSE_RIGHT ) ) {
            SDL_SetRelativeMouseMode( SDL_FALSE );
            SDL_ShowCursor( SDL_ENABLE );
            SDL_WarpMouseInWindow( m_window, saved_mouse_x, saved_mouse_y );
        }

        if ( m_dirtSwapchain ) {
            m_gfx->swapchain.Recreate( m_gfx->swapchain.extent.width, m_gfx->swapchain.extent.height );
            m_dirtSwapchain = false;
        }

        m_cameraController->Update( m_stats.frametime / 1000.0f );

        // do not draw if we are minimized
        if ( m_stopRendering ) {
            // throttle the speed to avoid the endless spinning
            std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
            continue;
        }

        UpdateScene( );

        ImGui_ImplVulkan_NewFrame( );
        ImGui_ImplSDL2_NewFrame( );
        ImGuizmo::SetOrthographic( false );

        ImGui::NewFrame( );
        ImGuizmo::BeginFrame( );

        if ( m_drawEditor ) {
            // dock space
            ImGui::DockSpaceOverViewport( 0, ImGui::GetMainViewport( ) );

            ImGui::ShowDemoWindow( );

            console.Draw( "Console", &m_open );

            {
                // Push a style to remove the window padding
                ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 0, 0 ) );
                if ( ImGui::Begin( "Viewport", 0, ImGuiWindowFlags_NoScrollbar ) ) {
                    const ImVec2 viewport_size = ImGui::GetContentRegionAvail( );

                    constexpr float aspect_ratio = 16.0f / 9.0f;
                    ImVec2 image_size;

                    if ( viewport_size.x / viewport_size.y > aspect_ratio ) {
                        image_size.y = viewport_size.y;
                        image_size.x = image_size.y * aspect_ratio;
                    }
                    else {
                        image_size.x = viewport_size.x;
                        image_size.y = image_size.x / aspect_ratio;
                    }

                    ImVec2 image_pos( ( viewport_size.x - image_size.x ) * 0.5f,
                                      ( viewport_size.y - image_size.y ) * 0.5f );

                    ImGui::SetCursorPos( image_pos );
                    ImGui::Image( reinterpret_cast<ImTextureID>( selected_set ), image_size );

                    // Overlay debug
                    {
                        auto text_pos = image_pos;
                        constexpr auto padding = 20u;
                        text_pos.x += padding;
                        text_pos.y += padding;

                        ImGui::SetCursorPos( text_pos );
                        ImGui::TextColored( ImVec4( 1, 1, 0, 1 ), "FPS: %.1f", ImGui::GetIO( ).Framerate );

                        text_pos.y += 20;
                        ImGui::SetCursorPos( text_pos );
                        ImGui::TextColored( ImVec4( 1, 1, 0, 1 ), "Frame: %d", m_gfx->swapchain.frameNumber );

                        text_pos.y += 20;
                        ImGui::SetCursorPos( text_pos );
                        ImGui::TextColored( ImVec4( 1, 1, 0, 1 ), "GPU: %s",
                                            m_gfx->deviceProperties.properties.deviceName );

                        text_pos.y += 20;
                        ImGui::SetCursorPos( text_pos );
                        ImGui::TextColored( ImVec4( 1, 1, 0, 1 ), "Image Codex: %d/%d",
                                            m_gfx->imageCodex.GetImages( ).size( ),
                                            m_gfx->imageCodex.bindlessRegistry.MaxBindlessImages );

                        auto window_pos = ImGui::GetWindowPos( );
                        ImVec2 position = { window_pos.x + image_pos.x,
                                            window_pos.y + image_pos.y + image_size.y - 450 };
                        m_visualProfiler.Render( position, ImVec2( 200, 450 ) );
                    }

                    if ( m_selectedNode != nullptr ) {
                        ImGuizmo::SetOrthographic( false );
                        ImGuizmo::SetDrawlist( );
                        ImGuizmo::SetRect( image_pos.x, image_pos.y, image_size.x, image_size.y );

                        auto camera_view = m_sceneData.view;
                        auto camera_proj = m_sceneData.proj;
                        camera_proj[1][1] *= -1;

                        auto tc = m_selectedNode->GetTransformMatrix( );
                        Manipulate( glm::value_ptr( camera_view ), glm::value_ptr( camera_proj ),
                                    ImGuizmo::OPERATION::UNIVERSAL, ImGuizmo::MODE::WORLD, value_ptr( tc ) );

                        Mat4 local_transform = tc;
                        if ( const auto parent = m_selectedNode->parent.lock( ) ) {
                            Mat4 parent_world_inverse = glm::inverse( parent->GetTransformMatrix( ) );
                            local_transform = parent_world_inverse * tc;
                        }

                        m_selectedNode->SetTransform( local_transform );
                    }
                }
                ImGui::End( );
                ImGui::PopStyleVar( );
            }

            if ( ImGui::Begin( "Scene" ) ) {
                DrawNodeHierarchy( m_scene->topNodes[0] );
            }
            ImGui::End( );

            if ( EG_INPUT.WasKeyPressed( EG_KEY::Z ) ) {
                ImGui::OpenPopup( "Viewport Context" );
            }
            if ( ImGui::BeginPopup( "Viewport Context" ) ) {
                ImGui::SeparatorText( "GBuffer" );
                if ( ImGui::RadioButton( "PBR Pass", &selected_set_n, 0 ) ) {
                    selected_set = m_gfx->swapchain.GetCurrentFrame( ).postProcessImage;
                }
                if ( ImGui::RadioButton( "Albedo", &selected_set_n, 1 ) ) {
                    selected_set = m_gfx->swapchain.GetCurrentFrame( ).gBuffer.albedo;
                }
                if ( ImGui::RadioButton( "Position", &selected_set_n, 2 ) ) {
                    selected_set = m_gfx->swapchain.GetCurrentFrame( ).gBuffer.position;
                }
                if ( ImGui::RadioButton( "Normal", &selected_set_n, 3 ) ) {
                    selected_set = m_gfx->swapchain.GetCurrentFrame( ).gBuffer.normal;
                }
                if ( ImGui::RadioButton( "PBR", &selected_set_n, 4 ) ) {
                    selected_set = m_gfx->swapchain.GetCurrentFrame( ).gBuffer.pbr;
                }
                if ( ImGui::RadioButton( "HDR", &selected_set_n, 5 ) ) {
                    selected_set = m_gfx->swapchain.GetCurrentFrame( ).hdrColor;
                }
                if ( ImGui::RadioButton( "ShadowMap", &selected_set_n, 6 ) ) {
                    selected_set = m_scene->directionalLights.at( 0 ).shadowMap;
                }
                if ( ImGui::RadioButton( "Depth", &selected_set_n, 7 ) ) {
                    selected_set = m_gfx->swapchain.GetCurrentFrame( ).depth;
                }
                if ( ImGui::RadioButton( "SSAO", &selected_set_n, 8 ) ) {
                    selected_set = m_gfx->swapchain.GetCurrentFrame( ).ssao;
                }
                ImGui::Separator( );
                ImGui::SliderFloat( "Render Scale", &m_renderScale, 0.3f, 1.f );
                ImGui::DragFloat( "Exposure", &m_ppConfig.exposure, 0.001f, 0.00f, 10.0f );
                ImGui::DragFloat( "Gamma", &m_ppConfig.gamma, 0.01f, 0.01f, 10.0f );
                ImGui::Checkbox( "Wireframe", &m_rendererOptions.wireframe );
                ImGui::Checkbox( "Render Irradiance Map", &m_rendererOptions.renderIrradianceInsteadSkybox );
                if ( ImGui::Checkbox( "VSync", &m_rendererOptions.vsync ) ) {
                    if ( m_rendererOptions.vsync ) {
                        m_gfx->swapchain.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
                    }
                    else {
                        m_gfx->swapchain.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
                    }
                    m_dirtSwapchain = true;
                }
                ImGui::EndPopup( );
            }

            if ( ImGui::Begin( "Settings" ) ) {
                if ( ImGui::CollapsingHeader( "Node" ) ) {
                    ImGui::Indent( );
                    if ( m_selectedNode ) {
                        if ( ImGui::Button( "Deselect" ) ) {
                            m_selectedNode = nullptr;
                        }

                        if ( m_selectedNode ) {
                            m_selectedNode->transform.DrawDebug( m_selectedNode->name );
                        }

                        for ( auto &light : m_scene->pointLights ) {
                            if ( light.node == m_selectedNode.get( ) ) {
                                light.DrawDebug( );
                            }
                        }
                    }
                    ImGui::Unindent( );
                }

                if ( ImGui::CollapsingHeader( "GPU Info" ) ) {
                    ImGui::Indent( );
                    m_gfx->DrawDebug( );
                    ImGui::Unindent( );
                }

                if ( ImGui::CollapsingHeader( "Camera" ) ) {
                    ImGui::SeparatorText( "Camera 3D" );
                    m_camera->DrawDebug( );

                    ImGui::SeparatorText( "Camera Controller" );
                    m_cameraController->DrawDebug( );
                }

                if ( ImGui::CollapsingHeader( "Renderer" ) ) {
                    ImGui::Indent( );

                    m_pbrPipeline.DrawDebug( );

                    if ( ImGui::CollapsingHeader( "SSAO" ) ) {
                        ImGui::Indent( );

                        ImGui::Checkbox( "SSAO", &m_ssaoSettings.enable );
                        if ( ImGui::DragFloat2( "Resolution", &m_rendererOptions.ssaoResolution.x, 1, 100,
                                                m_gfx->swapchain.extent.width ) ) {
                            for ( auto i = 0; i < Swapchain::FrameOverlap; i++ ) {
                                auto &image = m_gfx->imageCodex.GetImage( m_gfx->swapchain.frames[i].ssao );
                                image.Resize( VkExtent3D{ static_cast<uint32_t>( m_rendererOptions.ssaoResolution.x ),
                                                          static_cast<uint32_t>( m_rendererOptions.ssaoResolution.y ),
                                                          1 } );
                            }
                            VK_CHECK( vkWaitForFences( m_gfx->device, 1, &m_gfx->swapchain.GetCurrentFrame( ).fence,
                                                       true, 1000000000 ) );
                            m_ssaoPipeline.Cleanup( *m_gfx );

                            ActuallyConstructSsaoPipeline( );
                        }
                        ImGui::DragFloat( "SSAO Radius", &m_ssaoSettings.radius, 0.01f, 0.0f, 1.0f );
                        ImGui::DragFloat( "SSAO Bias", &m_ssaoSettings.bias, 0.01f, 0.0f, 1.0f );
                        ImGui::DragFloat( "SSAO Power", &m_ssaoSettings.power, 0.01f, 0.0f, 1.0f );

                        ImGui::Unindent( );
                    }

                    if ( ImGui::CollapsingHeader( "Frustum Culling" ) ) {
                        ImGui::Checkbox( "Enable", &m_rendererOptions.frustumCulling );
                        ImGui::Checkbox( "Freeze", &m_rendererOptions.useFrozenFrustum );
                        if ( ImGui::Button( "Reload Frozen Frustum" ) ) {
                            m_rendererOptions.lastSavedFrustum = m_camera->GetFrustum( );
                        }
                    }

                    ImGui::Unindent( );
                }

                if ( ImGui::CollapsingHeader( "Image Codex" ) ) {
                    ImGui::Indent( );
                    m_gfx->imageCodex.DrawDebug( );
                    ImGui::Unindent( );
                }

                if ( ImGui::CollapsingHeader( "Directional Lights" ) ) {
                    ImGui::Indent( );
                    for ( auto i = 0; i < m_scene->directionalLights.size( ); i++ ) {
                        if ( ImGui::CollapsingHeader( std::format( "Sun {}", i ).c_str( ) ) ) {
                            ImGui::PushID( i );

                            auto &light = m_scene->directionalLights.at( i );
                            ImGui::ColorEdit3( "Color HSV", &light.hsv.hue,
                                               ImGuiColorEditFlags_DisplayHSV | ImGuiColorEditFlags_InputHSV |
                                                       ImGuiColorEditFlags_PickerHueWheel );
                            ImGui::DragFloat( "Power", &light.power, 0.1f );

                            auto euler = glm::degrees( light.node->transform.euler );

                            if ( ImGui::DragFloat3( "Rotation", glm::value_ptr( euler ) ) ) {
                                m_rendererOptions.reRenderShadowMaps = true;
                                light.node->transform.euler = glm::radians( euler );
                            }

                            auto shadow_map_pos =
                                    glm::normalize( light.node->transform.AsMatrix( ) * glm::vec4( 0, 0, -1, 0 ) ) *
                                    light.distance;
                            ImGui::DragFloat3( "Pos", glm::value_ptr( shadow_map_pos ) );

                            m_rendererOptions.reRenderShadowMaps |= ImGui::DragFloat( "Distance", &light.distance );
                            m_rendererOptions.reRenderShadowMaps |= ImGui::DragFloat( "Right", &light.right );
                            m_rendererOptions.reRenderShadowMaps |= ImGui::DragFloat( "Up", &light.up );
                            m_rendererOptions.reRenderShadowMaps |= ImGui::DragFloat( "Near", &light.nearPlane );
                            m_rendererOptions.reRenderShadowMaps |= ImGui::DragFloat( "Far", &light.farPlane );

                            ImGui::Image( reinterpret_cast<ImTextureID>( light.shadowMap ), ImVec2( 200.0f, 200.0f ) );

                            ImGui::PopID( );
                        }
                    }
                    ImGui::Unindent( );
                }
            }
            ImGui::End( );

            if ( ImGui::Begin( "Stats" ) ) {
                ImGui::Text( "frametime %f ms", m_stats.frametime );
                ImGui::Text( "GPU: %f ms",
                             m_gfx->GetTimestampInMs( m_gfx->gpuTimestamps.at( 0 ), m_gfx->gpuTimestamps.at( 1 ) ) );
            }
            ImGui::End( );
        }

        if ( m_drawEditor == false && m_drawStats == true ) {
            auto extent = m_gfx->swapchain.extent;
            m_visualProfiler.Render( { 0, static_cast<float>( extent.height ) - 450 }, ImVec2( 200, 450 ) );
        }

        ImGui::Render( );

        Draw( );

        // get clock again, compare with start clock
        auto end = std::chrono::system_clock::now( );

        // convert to microseconds (integer), and then come back to milliseconds
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>( end - start );
        m_stats.frametime = elapsed.count( ) / 1000.f;

        if ( m_timer >= 500.0f ) {
            m_gfx->shaderStorage->Reconstruct( );
            m_timer = 0.0f;
        }

        m_timer += m_stats.frametime;
        const auto end_task = utils::GetTime( );
        m_visualProfiler.AddTimer( "Main", end_task - start_task, utils::VisualProfiler::Cpu );
    }
}

void VulkanEngine::DrawImGui( const VkCommandBuffer cmd, const VkImageView targetImageView ) {
    {
        using namespace vk_init;

        VkRenderingAttachmentInfo color_attachment = AttachmentInfo( targetImageView, nullptr );

        const VkRenderingInfo render_info = {
                .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                .pNext = nullptr,
                .renderArea = VkRect2D{ VkOffset2D{ 0, 0 }, m_drawExtent },
                .layerCount = 1,
                .colorAttachmentCount = 1,
                .pColorAttachments = &color_attachment,
                .pDepthAttachment = nullptr,
                .pStencilAttachment = nullptr,
        };
        vkCmdBeginRendering( cmd, &render_info );
    }

    m_imGuiPipeline.Draw( *m_gfx, cmd, ImGui::GetDrawData( ) );

    vkCmdEndRendering( cmd );
}

inline float dot( const Vec3 &v, const Vec4 &p ) { return v.x * p.x + v.y * p.y + v.z * p.z + p.w; }

bool IsVisible( const Mat4 &transform, const AABoundingBox *aabb, const Frustum &frustum ) {
    Vec3 points[] = {
            { aabb->min.x, aabb->min.y, aabb->min.z }, { aabb->max.x, aabb->min.y, aabb->min.z },
            { aabb->max.x, aabb->max.y, aabb->min.z }, { aabb->min.x, aabb->max.y, aabb->min.z },

            { aabb->min.x, aabb->min.y, aabb->max.z }, { aabb->max.x, aabb->min.y, aabb->max.z },
            { aabb->max.x, aabb->max.y, aabb->max.z }, { aabb->min.x, aabb->max.y, aabb->max.z },
    };

    // transform points to world space
    for ( int i = 0; i < 8; ++i ) {
        points[i] = transform * Vec4( points[i], 1.0f );
    }

    // for each plane…
    for ( int i = 0; i < 6; ++i ) {
        bool inside = false;

        for ( int j = 0; j < 8; ++j ) {
            if ( dot( Vec3( points[j] ), Vec3( frustum.planes[i] ) ) + frustum.planes[i].w > 0 ) {
                inside = true;
                break;
            }
        }

        if ( !inside ) {
            return false;
        }
    }

    return true;
}

void VulkanEngine::CreateDrawCommands( GfxDevice &gfx, const Scene &scene, const Node &node ) {
    if ( !node.meshIds.empty( ) ) {

        int i = 0;
        for ( const auto mesh_id : node.meshIds ) {
            auto model = node.GetTransformMatrix( );

            auto &mesh_asset = scene.meshes[mesh_id];
            auto &mesh = gfx.meshCodex.GetMesh( mesh_asset.mesh );
            MeshDrawCommand mdc = { .indexBuffer = mesh.indexBuffer.buffer,
                                    .indexCount = mesh.indexCount,
                                    .vertexBufferAddress = mesh.vertexBufferAddress,
                                    .worldFromLocal = model,
                                    .materialId = scene.materials[mesh_asset.material] };
            m_shadowMapCommands.push_back( mdc );

            if ( m_rendererOptions.frustumCulling ) {
                auto &aabb = node.boundingBoxes[i++];
                auto visible = IsVisible( model, &aabb,
                                          m_rendererOptions.useFrozenFrustum ? m_rendererOptions.lastSavedFrustum
                                                                             : m_camera->GetFrustum( ) );
                if ( !visible ) {
                    continue;
                }
                m_drawCommands.push_back( mdc );
            }
        }
    }

    for ( auto &n : node.children ) {
        CreateDrawCommands( gfx, scene, *n.get( ) );
    }
}

void VulkanEngine::UpdateScene( ) {
    ZoneScopedN( "update_scene" );

    m_drawCommands.clear( );
    m_shadowMapCommands.clear( );
    {
        auto start_commands = utils::GetTime( );
        CreateDrawCommands( *m_gfx.get( ), *m_scene, *( m_scene->topNodes[0].get( ) ) );
        auto end_commands = utils::GetTime( );
        m_visualProfiler.AddTimer( "Create Commands", end_commands - start_commands, utils::VisualProfiler::Cpu );
    }

    // camera
    auto start = utils::GetTime( );

    m_sceneData.view = m_camera->GetViewMatrix( );
    m_sceneData.proj = m_camera->GetProjectionMatrix( );
    m_sceneData.viewproj = m_sceneData.proj * m_sceneData.view;
    m_sceneData.cameraPosition = Vec4( m_camera->GetPosition( ), 0.0f );

    m_gpuDirectionalLights.clear( );
    m_sceneData.numberOfDirectionalLights = static_cast<int>( m_scene->directionalLights.size( ) );
    for ( auto i = 0; i < m_scene->directionalLights.size( ); i++ ) {
        GpuDirectionalLight gpu_light;
        auto &light = m_scene->directionalLights.at( i );

        ImGui::ColorConvertHSVtoRGB( light.hsv.hue, light.hsv.saturation, light.hsv.value, gpu_light.color.x,
                                     gpu_light.color.y, gpu_light.color.z );
        gpu_light.color *= light.power;

        // get direction
        auto direction = light.node->GetTransformMatrix( ) * glm::vec4( 0, 0, 1, 0 );
        gpu_light.direction = direction;

        auto proj = glm::ortho( -light.right, light.right, -light.up, light.up, light.nearPlane, light.farPlane );
        auto shadow_map_pos =
                glm::normalize( light.node->GetTransformMatrix( ) * glm::vec4( 0, 0, 1, 0 ) ) * light.distance;
        auto view = glm::lookAt( Vec3( shadow_map_pos ), Vec3( 0.0f, 0.0f, 0.0f ), GLOBAL_UP );
        gpu_light.proj = proj;
        gpu_light.view = view;

        gpu_light.shadowMap = light.shadowMap;

        m_gpuDirectionalLights.push_back( gpu_light );
    }

    m_gpuPointLights.clear( );
    m_sceneData.numberOfPointLights = static_cast<int>( m_scene->pointLights.size( ) );
    for ( auto i = 0; i < m_scene->pointLights.size( ); i++ ) {
        GpuPointLightData gpu_light;
        auto &light = m_scene->pointLights.at( i );

        gpu_light.position = light.node->transform.position;

        ImGui::ColorConvertHSVtoRGB( light.hsv.hue, light.hsv.saturation, light.hsv.value, gpu_light.color.x,
                                     gpu_light.color.y, gpu_light.color.z );
        gpu_light.color *= light.power;

        gpu_light.quadratic = light.quadratic;
        gpu_light.linear = light.linear;
        gpu_light.constant = light.constant;

        m_gpuPointLights.push_back( gpu_light );
    }

    m_gpuSceneData.Upload( *m_gfx, &m_sceneData, sizeof( GpuSceneData ) );

    m_ssaoBuffer.Upload( *m_gfx, &m_ssaoSettings, sizeof( SsaoSettings ) );

    const auto end = utils::GetTime( );
    m_visualProfiler.AddTimer( "Scene", end - start, utils::VisualProfiler::Cpu );
}
