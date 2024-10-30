/*****************************************************************************
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
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_vulkan.h>
#include "SDL_events.h"
#include "SDL_stdinc.h"
#include "SDL_video.h"
#include "fmt/core.h"
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/gtx/integer.hpp"
#include "glm/gtx/matrix_decompose.hpp"
#include "glm/packing.hpp"

#include "vk_mem_alloc.h"

#include <glm/gtc/type_ptr.hpp>

#include <graphics/pipelines/compute_pipeline.h>
#include <tracy/Tracy.hpp>
#include "engine/input.h"
#include "imguizmo/ImGuizmo.h"

#include <engine/loader.h>
#include <glm/gtx/euler_angles.hpp>

#include <utils/ImGuiProfilerRenderer.h>
#include "graphics/draw_command.h"

#include <graphics/tl_renderer.h>

TL_Engine *g_TL = nullptr;
utils::VisualProfiler g_visualProfiler = utils::VisualProfiler( 300 );


// TODO: move
void GpuBuffer::Upload( const TL_VkContext &vkctx, const void *data, const size_t size ) const {
    void *mapped_buffer = { };

    vmaMapMemory( vkctx.allocator, allocation, &mapped_buffer );
    memcpy( mapped_buffer, data, size );
    vmaUnmapMemory( vkctx.allocator, allocation );
}

VkDeviceAddress GpuBuffer::GetDeviceAddress( const TL_VkContext &vkctx ) {
    if ( deviceAddress == 0 ) {
        const VkBufferDeviceAddressInfo address_info = {
                .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                .pNext = nullptr,
                .buffer = buffer,
        };

        deviceAddress = vkGetBufferDeviceAddress( vkctx.device, &address_info );
    }

    return deviceAddress;
}

TL_Engine &TL_Engine::Get( ) { return *g_TL; }

using namespace TL;

void TL_Engine::Init( ) {
    assert( g_TL == nullptr );
    g_TL = this;

    InitSdl( );
    InitRenderer( );
    InitDefaultData( );
    InitImGui( );
    m_imGuiPipeline.Init( *vkctx );
    EG_INPUT.Init( );
    InitScene( );

    g_visualProfiler.RegisterTask( "GBuffer", utils::colors::CARROT, utils::VisualProfiler::Cpu );
    g_visualProfiler.RegisterTask( "Create Commands", utils::colors::EMERALD, utils::VisualProfiler::Cpu );
    g_visualProfiler.RegisterTask( "Scene", utils::colors::EMERALD, utils::VisualProfiler::Cpu );

    g_visualProfiler.RegisterTask( "ShadowMap", utils::colors::TURQUOISE, utils::VisualProfiler::Gpu );
    g_visualProfiler.RegisterTask( "GBuffer", utils::colors::ALIZARIN, utils::VisualProfiler::Gpu );
    g_visualProfiler.RegisterTask( "Lighting", utils::colors::AMETHYST, utils::VisualProfiler::Gpu );
    g_visualProfiler.RegisterTask( "Skybox", utils::colors::SUN_FLOWER, utils::VisualProfiler::Gpu );
    g_visualProfiler.RegisterTask( "Post Process", utils::colors::PETER_RIVER, utils::VisualProfiler::Gpu );

    m_isInitialized = true;
}

void TL_Engine::InitSdl( ) {
    SDL_Init( SDL_INIT_VIDEO );

    constexpr auto window_flags = static_cast<SDL_WindowFlags>( SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE );

    m_window = SDL_CreateWindow( "Vulkan Engine", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                 m_windowExtent.width, m_windowExtent.height, window_flags );
}

void TL_Engine::InitRenderer( ) {
    renderer = std::make_unique<TL::Renderer>( );
    renderer->Init( m_window, { WIDTH, HEIGHT } );

    m_mainDeletionQueue.Flush( );
}

void TL_Engine::InitImGui( ) {
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
    VKCALL( vkCreateDescriptorPool( vkctx->device, &pool_info, nullptr, &imgui_pool ) );

    ImGui::CreateContext( );

    ImGui_ImplSDL2_InitForVulkan( m_window );

    ImGui_ImplVulkan_InitInfo init_info = {
            .Instance = vkctx->instance,
            .PhysicalDevice = vkctx->chosenGpu,
            .Device = vkctx->device,
            .Queue = vkctx->graphicsQueue,
            .DescriptorPool = imgui_pool,
            .MinImageCount = 3,
            .ImageCount = 3,
            .UseDynamicRendering = true,
    };

    init_info.PipelineRenderingCreateInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
    init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats = &vkctx->format;
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
        vkDestroyDescriptorPool( vkctx->device, imgui_pool, nullptr );
    } );
}

auto RandomRange( const float min, const float max ) -> float {
    static std::random_device rd;
    static std::mt19937 gen( rd( ) );

    std::uniform_real_distribution<float> dis( min, max );

    return dis( gen );
}

float Lerp( const float a, const float b, const float f ) { return a + f * ( b - a ); }

void TL_Engine::ResizeSwapchain( uint32_t width, uint32_t height ) {
    vkDeviceWaitIdle( vkctx->device );

    m_windowExtent.width = width;
    m_windowExtent.height = height;

    // TODO:
    // vkctx->Recreate( width, height );
}

void TL_Engine::Cleanup( ) {
    if ( m_isInitialized ) {
        // wait for gpu work to finish
        vkDeviceWaitIdle( vkctx->device );

        renderer->Cleanup( );

        m_pbrPipeline.Cleanup( *vkctx );
        m_wireframePipeline.Cleanup( *vkctx );
        m_imGuiPipeline.Cleanup( *vkctx );
        m_skyboxPipeline.Cleanup( *vkctx );
        m_blurPipeline.Cleanup( *vkctx );

        m_postProcessPipeline.Cleanup( *vkctx );

        m_ibl.Clean( *vkctx );

        m_mainDeletionQueue.Flush( );

        vkctx->Cleanup( );

        SDL_DestroyWindow( m_window );
    }

    // clear engine pointer
    g_TL = nullptr;
}

void TL_Engine::Draw( ) {
    ZoneScopedN( "draw" );

    m_stats.drawcallCount = 0;
    m_stats.triangleCount = 0;

    auto frame = vkctx->GetCurrentFrame( );

    frame.deletionQueue.Flush( );
    renderer->StartFrame( );
    const auto cmd = frame.commandBuffer;

    const auto &depth = vkctx->imageCodex.GetImage( vkctx->GetCurrentFrame( ).depth );

    depth.TransitionLayout( cmd, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, true );

    //if ( m_rendererOptions.reRenderShadowMaps ) {
    //    m_rendererOptions.reRenderShadowMaps = false;
    //    ShadowMapPass( cmd );
    //}

    renderer->Frame( );

    vkCmdWriteTimestamp( cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, frame.queryPoolTimestamps, 4 );
    PbrPass( cmd );
    vkCmdWriteTimestamp( cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, frame.queryPoolTimestamps, 5 );

    vkCmdWriteTimestamp( cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, frame.queryPoolTimestamps, 6 );
    SkyboxPass( cmd );
    vkCmdWriteTimestamp( cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, frame.queryPoolTimestamps, 7 );

    vkCmdWriteTimestamp( cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, frame.queryPoolTimestamps, 8 );
    PostProcessPass( cmd );
    vkCmdWriteTimestamp( cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, frame.queryPoolTimestamps, 9 );

    {
        ZoneScopedN( "Final Image" );
        if ( m_drawEditor ) {
            DrawImGui( cmd, vkctx->views[renderer->swapchainImageIndex] );
            image::TransitionLayout( cmd, vkctx->images[renderer->swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED,
                                     VK_IMAGE_LAYOUT_PRESENT_SRC_KHR );
        }
        else {
            auto &ppi = vkctx->imageCodex.GetImage( vkctx->GetCurrentFrame( ).postProcessImage );
            ppi.TransitionLayout( cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL );

            image::TransitionLayout( cmd, vkctx->images[renderer->swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED,
                                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL );

            image::Blit( cmd, ppi.GetImage( ), { ppi.GetExtent( ).width, ppi.GetExtent( ).height },
                         vkctx->images[renderer->swapchainImageIndex], vkctx->extent );
            if ( m_drawStats ) {
                DrawImGui( cmd, vkctx->views[renderer->swapchainImageIndex] );
            }

            image::TransitionLayout( cmd, vkctx->images[renderer->swapchainImageIndex],
                                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR );
        }
    }

    // send commands
    renderer->EndFrame( );
    renderer->Present( );

    // increase frame number for next loop
    vkctx->frameNumber++;
}

void TL_Engine::PbrPass( VkCommandBuffer cmd ) const {
    ZoneScopedN( "PBR Pass" );
    START_LABEL( cmd, "PBR Pass", Vec4( 1.0f, 0.0f, 1.0f, 1.0f ) );

    using namespace vk_init;

    {
        auto &image = vkctx->imageCodex.GetImage( vkctx->GetCurrentFrame( ).hdrColor );

        VkClearValue clear_color = { 0.0f, 0.0f, 0.0f, 0.0f };
        VkRenderingAttachmentInfo color_attachment = AttachmentInfo( image.GetBaseView( ), &clear_color );

        VkRenderingInfo render_info = { .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                                        .pNext = nullptr,
                                        .renderArea = VkRect2D{ VkOffset2D{ 0, 0 }, vkctx->extent },
                                        .layerCount = 1,
                                        .colorAttachmentCount = 1,
                                        .pColorAttachments = &color_attachment,
                                        .pDepthAttachment = nullptr,
                                        .pStencilAttachment = nullptr };
        vkCmdBeginRendering( cmd, &render_info );
    }

    m_pbrPipeline.Draw( *vkctx, cmd, m_sceneData, m_gpuDirectionalLights, m_gpuPointLights,
                        vkctx->GetCurrentFrame( ).gBuffer, m_ibl.GetIrradiance( ), m_ibl.GetRadiance( ),
                        m_ibl.GetBrdf( ) );

    vkCmdEndRendering( cmd );

    END_LABEL( cmd );
}

void TL_Engine::SkyboxPass( VkCommandBuffer cmd ) const {
    ZoneScopedN( "Skybox Pass" );
    START_LABEL( cmd, "Skybox Pass", Vec4( 0.0f, 1.0f, 0.0f, 1.0f ) );

    using namespace vk_init;

    {
        auto &image = vkctx->imageCodex.GetImage( vkctx->GetCurrentFrame( ).hdrColor );
        auto &depth = vkctx->imageCodex.GetImage( vkctx->GetCurrentFrame( ).depth );

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
                                        .renderArea = VkRect2D{ VkOffset2D{ 0, 0 }, vkctx->extent },
                                        .layerCount = 1,
                                        .colorAttachmentCount = 1,
                                        .pColorAttachments = &color_attachment,
                                        .pDepthAttachment = &depth_attachment,
                                        .pStencilAttachment = nullptr };
        vkCmdBeginRendering( cmd, &render_info );
    }

    if ( m_rendererOptions.renderIrradianceInsteadSkybox ) {
        m_skyboxPipeline.Draw( *vkctx, cmd, m_ibl.GetIrradiance( ), m_sceneData );
    }
    else {
        m_skyboxPipeline.Draw( *vkctx, cmd, m_ibl.GetSkybox( ), m_sceneData );
    }

    vkCmdEndRendering( cmd );

    END_LABEL( cmd );
}

void TL_Engine::PostProcessPass( VkCommandBuffer cmd ) const {
    auto bindless = vkctx->GetBindlessSet( );
    auto &output = vkctx->imageCodex.GetImage( vkctx->GetCurrentFrame( ).postProcessImage );

    m_ppConfig.hdr = vkctx->GetCurrentFrame( ).hdrColor;

    output.TransitionLayout( cmd, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL );

    m_postProcessPipeline.Bind( cmd );
    m_postProcessPipeline.BindDescriptorSet( cmd, bindless, 0 );
    m_postProcessPipeline.BindDescriptorSet( cmd, m_postProcessSet.GetCurrentFrame( ), 1 );
    m_postProcessPipeline.PushConstants( cmd, sizeof( PostProcessConfig ), &m_ppConfig );
    m_postProcessPipeline.Dispatch( cmd, ( output.GetExtent( ).width + 15 ) / 16,
                                    ( output.GetExtent( ).height + 15 ) / 16, 6 );

    // output.TransitionLayout( cmd, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
}

void TL_Engine::InitDefaultData( ) {
    InitImages( );

    m_gpuSceneData = vkctx->Allocate( sizeof( TL::GpuSceneData ),
                                      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                      VMA_MEMORY_USAGE_CPU_TO_GPU, "drawGeometry" );

    m_mainDeletionQueue.PushFunction( [=, this]( ) { vkctx->Free( m_gpuSceneData ); } );

    m_pbrPipeline.Init( *vkctx );
    m_wireframePipeline.Init( *vkctx );
    m_skyboxPipeline.Init( *vkctx );

    // post process pipeline
    auto &post_process_shader = vkctx->shaderStorage->Get( "post_process", TCompute );
    post_process_shader.RegisterReloadCallback( [&]( VkShaderModule shader ) {
        VKCALL( vkWaitForFences( vkctx->device, 1, &vkctx->GetCurrentFrame( ).fence, true, 1000000000 ) );
        m_postProcessPipeline.Cleanup( *vkctx );

        m_postProcessPipeline.AddDescriptorSetLayout( 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE );
        m_postProcessPipeline.AddPushConstantRange( sizeof( PostProcessConfig ) );
        m_postProcessPipeline.Build( *vkctx, post_process_shader.handle, "post process compute" );
        m_postProcessSet = vkctx->AllocateMultiSet( m_postProcessPipeline.GetLayout( ) );

        // TODO: this every frame for more than one inflight frame
        for ( auto i = 0; i < TL_VkContext::FrameOverlap; i++ ) {
            DescriptorWriter writer;
            auto &out_image = vkctx->imageCodex.GetImage( vkctx->GetCurrentFrame( ).postProcessImage );
            writer.WriteImage( 0, out_image.GetBaseView( ), nullptr, VK_IMAGE_LAYOUT_GENERAL,
                               VK_DESCRIPTOR_TYPE_STORAGE_IMAGE );
            writer.UpdateSet( vkctx->device, m_postProcessSet.m_sets[i] );
        }
    } );

    m_postProcessPipeline.AddDescriptorSetLayout( 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE );
    m_postProcessPipeline.AddPushConstantRange( sizeof( PostProcessConfig ) );
    m_postProcessPipeline.Build( *vkctx, post_process_shader.handle, "post process compute" );
    m_postProcessSet = vkctx->AllocateMultiSet( m_postProcessPipeline.GetLayout( ) );

    // TODO: this every frame for more than one inflight frame
    for ( auto i = 0; i < TL_VkContext::FrameOverlap; i++ ) {
        DescriptorWriter writer;
        auto &out_image = vkctx->imageCodex.GetImage( vkctx->GetCurrentFrame( ).postProcessImage );
        writer.WriteImage( 0, out_image.GetBaseView( ), nullptr, VK_IMAGE_LAYOUT_GENERAL,
                           VK_DESCRIPTOR_TYPE_STORAGE_IMAGE );
        writer.UpdateSet( vkctx->device, m_postProcessSet.m_sets[i] );
    }
}

void TL_Engine::InitImages( ) {
    // 3 default textures, white, grey, black. 1 pixel each
    uint32_t white = glm::packUnorm4x8( glm::vec4( 1, 1, 1, 1 ) );
    m_whiteImage = vkctx->imageCodex.LoadImageFromData( "debug_white_img", ( void * )&white, VkExtent3D{ 1, 1, 1 },
                                                        VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, false );

    uint32_t grey = glm::packUnorm4x8( glm::vec4( 0.66f, 0.66f, 0.66f, 1 ) );
    m_greyImage = vkctx->imageCodex.LoadImageFromData( "debug_grey_img", ( void * )&grey, VkExtent3D{ 1, 1, 1 },
                                                       VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, false );

    uint32_t black = glm::packUnorm4x8( glm::vec4( 0, 0, 0, 0 ) );
    m_blackImage = vkctx->imageCodex.LoadImageFromData( "debug_black_img", ( void * )&white, VkExtent3D{ 1, 1, 1 },
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
            vkctx->imageCodex.LoadImageFromData( "debug_checkboard_img", ( void * )&white, VkExtent3D{ 16, 16, 1 },
                                                 VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, false );
}

void TL_Engine::InitScene( ) {
    m_ibl.Init( *vkctx, "../../assets/texture/ibls/belfast_sunset_4k.hdr" );

    m_scene = GltfLoader::Load( *vkctx, "../../assets/sponza.glb" );

    // Use camera from renderer
    m_camera = renderer->GetCamera( );
    m_fpsController = std::make_unique<FirstPersonFlyingController>( m_camera.get( ), 0.1f, 5.0f );
    m_cameraController = m_fpsController.get( );
    
    if ( m_scene->directionalLights.size( ) != 0 ) {
        auto &light = m_scene->directionalLights.at( 0 );
        light.power = 30.0f;
        light.distance = 100.0f;
        light.right = 115.0f;
        light.up = 115.0f;
        light.farPlane = 131.0f;
    }
}

void TL_Engine::DrawNodeHierarchy( const std::shared_ptr<Node> &node ) {
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

void TL_Engine::Run( ) {
    bool b_quit = false;

    static ImageId selected_set = vkctx->GetCurrentFrame( ).postProcessImage;
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
            // TODO:
            // vkctx->Recreate( vkctx->extent.width, vkctx->extent.height );
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

        {
            ZoneScopedN( "ImGui Menu" );
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
                        ImGui::Image( ( ImTextureID ) static_cast<uintptr_t>( selected_set ), image_size );

                        // Overlay debug
                        {
                            auto text_pos = image_pos;
                            constexpr auto padding = 20u;
                            text_pos.x += padding;
                            text_pos.y += padding;

                            ImGui::SetCursorPos( text_pos );
                            ImGui::TextColored( ImVec4( 1, 0, 0, 1 ), "FPS: %.1f", ImGui::GetIO( ).Framerate );

                            text_pos.y += 20;
                            ImGui::SetCursorPos( text_pos );
                            ImGui::TextColored( ImVec4( 1, 0, 0, 1 ), "Frame: %d", vkctx->frameNumber );

                            text_pos.y += 20;
                            ImGui::SetCursorPos( text_pos );
                            ImGui::TextColored( ImVec4( 1, 0, 0, 1 ), "GPU: %s",
                                                vkctx->deviceProperties.properties.deviceName );

                            text_pos.y += 20;
                            ImGui::SetCursorPos( text_pos );
                            ImGui::TextColored( ImVec4( 1, 0, 0, 1 ), "Image Codex: %d/%d",
                                                vkctx->imageCodex.GetImages( ).size( ),
                                                vkctx->imageCodex.bindlessRegistry.MaxBindlessImages );

                            text_pos.y += 20;
                            ImGui::SetCursorPos( text_pos );
                            ImGui::TextColored( ImVec4( 1, 0, 0, 1 ), "Triangles: %uld", m_stats.triangleCount );

                            text_pos.y += 20;
                            ImGui::SetCursorPos( text_pos );
                            ImGui::TextColored( ImVec4( 1, 0, 0, 1 ), "Draw Calls: %d", m_stats.drawcallCount );

                            auto window_pos = ImGui::GetWindowPos( );
                            ImVec2 position = { window_pos.x + image_pos.x,
                                                window_pos.y + image_pos.y + image_size.y - 450 };
                            g_visualProfiler.Render( position, ImVec2( 200, 450 ) );
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
                    for ( auto &node : m_scene->topNodes ) {
                        DrawNodeHierarchy( node );
                    }
                }
                ImGui::End( );

                if ( EG_INPUT.WasKeyPressed( EG_KEY::Z ) ) {
                    ImGui::OpenPopup( "Viewport Context" );
                }
                if ( ImGui::BeginPopup( "Viewport Context" ) ) {
                    ImGui::SeparatorText( "GBuffer" );
                    if ( ImGui::RadioButton( "PBR Pass", &selected_set_n, 0 ) ) {
                        selected_set = vkctx->GetCurrentFrame( ).postProcessImage;
                    }
                    if ( ImGui::RadioButton( "Albedo", &selected_set_n, 1 ) ) {
                        selected_set = vkctx->GetCurrentFrame( ).gBuffer.albedo;
                    }
                    if ( ImGui::RadioButton( "Position", &selected_set_n, 2 ) ) {
                        selected_set = vkctx->GetCurrentFrame( ).gBuffer.position;
                    }
                    if ( ImGui::RadioButton( "Normal", &selected_set_n, 3 ) ) {
                        selected_set = vkctx->GetCurrentFrame( ).gBuffer.normal;
                    }
                    if ( ImGui::RadioButton( "PBR", &selected_set_n, 4 ) ) {
                        selected_set = vkctx->GetCurrentFrame( ).gBuffer.pbr;
                    }
                    if ( ImGui::RadioButton( "HDR", &selected_set_n, 5 ) ) {
                        selected_set = vkctx->GetCurrentFrame( ).hdrColor;
                    }
                    if ( ImGui::RadioButton( "ShadowMap", &selected_set_n, 6 ) ) {
                        selected_set = m_scene->directionalLights.at( 0 ).shadowMap;
                    }
                    if ( ImGui::RadioButton( "Depth", &selected_set_n, 7 ) ) {
                        selected_set = vkctx->GetCurrentFrame( ).depth;
                    }
                    ImGui::Separator( );
                    ImGui::DragFloat( "Exposure", &m_ppConfig.exposure, 0.001f, 0.00f, 10.0f );
                    ImGui::DragFloat( "Gamma", &m_ppConfig.gamma, 0.01f, 0.01f, 10.0f );
                    ImGui::Checkbox( "Wireframe", &m_rendererOptions.wireframe );
                    ImGui::Checkbox( "Render Irradiance Map", &m_rendererOptions.renderIrradianceInsteadSkybox );
                    if ( ImGui::Checkbox( "VSync", &m_rendererOptions.vsync ) ) {
                        if ( m_rendererOptions.vsync ) {
                            vkctx->presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
                        }
                        else {
                            vkctx->presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
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
                                ImGui::Text( "LOD rendering: %d", m_selectedNode->currentLod );
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
                        vkctx->DrawDebug( );
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

                        if ( ImGui::CollapsingHeader( "Frustum Culling" ) ) {
                            ImGui::Indent( );
                            ImGui::Checkbox( "Enable", &m_rendererOptions.frustumCulling );
                            ImGui::Checkbox( "Freeze", &m_rendererOptions.useFrozenFrustum );
                            if ( ImGui::Button( "Reload Frozen Frustum" ) ) {
                                m_rendererOptions.lastSavedFrustum = m_camera->GetFrustum( );
                            }
                            ImGui::Unindent( );
                        }

                        if ( ImGui::CollapsingHeader( "LOD System" ) ) {
                            ImGui::Indent( );
                            ImGui::PushID( "LOD" );
                            ImGui::Checkbox( "Enable", &m_rendererOptions.lodSystem );
                            ImGui::Checkbox( "Freeze", &m_rendererOptions.freezeLodSystem );
                            ImGui::PopID( );
                            ImGui::Unindent( );
                        }

                        ImGui::Unindent( );
                    }

                    if ( ImGui::CollapsingHeader( "Image Codex" ) ) {
                        ImGui::Indent( );
                        vkctx->imageCodex.DrawDebug( );
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

                                ImGui::Image( ( ImTextureID ) static_cast<uintptr_t>( light.shadowMap ),
                                              ImVec2( 200.0f, 200.0f ) );

                                ImGui::PopID( );
                            }
                        }
                        ImGui::Unindent( );
                    }
                }
                ImGui::End( );

                if ( ImGui::Begin( "Stats" ) ) {
                    ImGui::Text( "frametime %f ms", m_stats.frametime );
                }
                ImGui::End( );
            }

            if ( m_drawEditor == false && m_drawStats == true ) {
                auto extent = vkctx->extent;
                g_visualProfiler.Render( { 0, static_cast<float>( extent.height ) - 450 }, ImVec2( 200, 450 ) );
            }

            ImGui::Render( );
        }

        Draw( );

        // get clock again, compare with start clock
        auto end = std::chrono::system_clock::now( );
        // convert to microseconds (integer), and then come back to milliseconds
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>( end - start );
        m_stats.frametime = elapsed.count( ) / 1000.f;

        if ( m_timer >= 500.0f ) {
            vkctx->shaderStorage->Reconstruct( );
            m_timer = 0.0f;
        }

        m_timer += m_stats.frametime;
        const auto end_task = utils::GetTime( );
        // g_visualProfiler.AddTimer( "Main", end_task - start_task, utils::VisualProfiler::Cpu );
    }
}

void TL_Engine::DrawImGui( const VkCommandBuffer cmd, const VkImageView targetImageView ) {
    {
        using namespace vk_init;

        VkRenderingAttachmentInfo color_attachment = AttachmentInfo( targetImageView, nullptr );

        const VkRenderingInfo render_info = {
                .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                .pNext = nullptr,
                .renderArea = VkRect2D{ VkOffset2D{ 0, 0 }, vkctx->extent },
                .layerCount = 1,
                .colorAttachmentCount = 1,
                .pColorAttachments = &color_attachment,
                .pDepthAttachment = nullptr,
                .pStencilAttachment = nullptr,
        };
        vkCmdBeginRendering( cmd, &render_info );
    }

    m_imGuiPipeline.Draw( *vkctx, cmd, ImGui::GetDrawData( ) );

    vkCmdEndRendering( cmd );
}

inline float dot( const Vec3 &v, const Vec4 &p ) { return v.x * p.x + v.y * p.y + v.z * p.z + p.w; }

VisibilityLODResult TL_Engine::VisibilityCheckWithLOD( const Mat4 &transform, const AABoundingBox *aabb,
                                                       const Frustum &frustum ) {

    // If neither frustum or lod is enable, then everything is visible and finest lod
    if ( !m_rendererOptions.frustumCulling && !m_rendererOptions.lodSystem ) {
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
        if ( m_rendererOptions.lodSystem ) {
            clips[i] = m_camera->GetProjectionMatrix( ) * m_camera->GetViewMatrix( ) * Vec4( points[i], 1.0f );
        }
    }

    bool is_visible = true;

    if ( m_rendererOptions.frustumCulling ) {
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

    if ( m_rendererOptions.lodSystem ) {
        float min_x = std::numeric_limits<float>::max( );
        float max_x = std::numeric_limits<float>::lowest( );
        float min_y = std::numeric_limits<float>::max( );
        float max_y = std::numeric_limits<float>::lowest( );

        for ( int i = 0; i < 8; i++ ) {
            Vec4 clip = clips[i];
            Vec3 ndc = Vec3( clip ) / clip.w;

            ndc = glm::clamp( ndc, -1.0f, 1.0f );
            Vec2 screen = Vec2( ( ndc.x + 1.0f ) * 0.5f * vkctx->extent.width,
                                ( 1.0f - ndc.y ) * 0.5f * vkctx->extent.height );

            min_x = std::min( min_x, screen.x );
            max_x = std::max( max_x, screen.x );
            min_y = std::min( min_y, screen.y );
            max_y = std::max( max_y, screen.y );
        }

        float width = max_x - min_x;
        float height = max_y - min_y;
        float screen_size = std::max( width, height );

        constexpr float lodThresholds[5] = { 250.0f, 170.0f, 100.0f, 50.0f, 20.0f };
        int selected_lod = 5;
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

void TL_Engine::CreateDrawCommands( TL_VkContext &vkctx, const Scene &scene, Node &node ) {
    if ( !node.meshIds.empty( ) ) {

        int i = 0;
        for ( const auto mesh_id : node.meshIds ) {
            auto model = node.GetTransformMatrix( );

            auto &mesh_asset = scene.meshes[mesh_id];
            auto &mesh = vkctx.meshCodex.GetMesh( mesh_asset.mesh );
            MeshDrawCommand mdc = {
                    .indexBuffer = mesh.indexBuffer[0].buffer,
                    .indexCount = mesh.indexCount[0],
                    .vertexBufferAddress = mesh.vertexBufferAddress,
                    .worldFromLocal = model,
                    .materialId = scene.materials[mesh_asset.material],
            };
            if ( m_rendererOptions.reRenderShadowMaps ) {
                m_shadowMapCommands.push_back( mdc );
            }

            if ( m_rendererOptions.frustumCulling ) {
                auto &aabb = node.boundingBoxes[i++];
                auto visibility =
                        VisibilityCheckWithLOD( model, &aabb,
                                                m_rendererOptions.useFrozenFrustum ? m_rendererOptions.lastSavedFrustum
                                                                                   : m_camera->GetFrustum( ) );
                if ( !visibility.isVisible ) {
                    continue;
                }

                // Do not update the current LOD for the node if freeze LOD system is toggled
                if ( !m_rendererOptions.freezeLodSystem ) {
                    node.currentLod = std::min( ( int )( mesh.indexCount.size( ) - 1 ), visibility.lodLevelToRender );
                }

                mdc.indexBuffer = mesh.indexBuffer[node.currentLod].buffer;
                mdc.indexCount = mesh.indexCount[node.currentLod];
                m_drawCommands.push_back( mdc );
            }
            else {
                m_drawCommands.push_back( mdc );
            }
        }
    }

    for ( auto &n : node.children ) {
        CreateDrawCommands( vkctx, scene, *n.get( ) );
    }
}

void TL_Engine::UpdateScene( ) {
    ZoneScopedN( "update_scene" );

    m_drawCommands.clear( );
    m_shadowMapCommands.clear( );
    {
        auto start_commands = utils::GetTime( );
        for ( auto &node : m_scene->topNodes ) {
            CreateDrawCommands( *vkctx, *m_scene, *node );
        }
        auto end_commands = utils::GetTime( );
        g_visualProfiler.AddTimer( "Create Commands", end_commands - start_commands, utils::VisualProfiler::Cpu );
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

    m_gpuSceneData.Upload( *vkctx, &m_sceneData, sizeof( ::GpuSceneData ) );

    const auto end = utils::GetTime( );
    g_visualProfiler.AddTimer( "Scene", end - start, utils::VisualProfiler::Cpu );
}
