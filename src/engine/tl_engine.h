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

#pragma once

#include <engine/scene.h>
#include <graphics/pipelines/imgui_pipeline.h>
#include <graphics/utils/vk_types.h>
#include <utils/ImGuiProfilerRenderer.h>
#include <utils/imgui_console.h>
#include <vulkan/vulkan_core.h>
#include "../camera/camera.h"
#include "../graphics/resources/r_resources.h"
#include "../graphics/tl_renderer.h"
#include "../utils/profiler.h"

#include <world/tl_scene.h>

class TL_Engine;

extern TL_Engine*            g_TL;
extern utils::VisualProfiler g_visualProfiler;

class TL_Engine {
public:
    static TL_Engine& Get( );

    void Init( );
    void Cleanup( );
    void Draw( );
    void Run( );

    void ResizeSwapchain( uint32_t width, uint32_t height );

    ImGuiConsole                  console;
    std::unique_ptr<TL::Renderer> renderer = nullptr;

    void InitSdl( );
    void InitImGui( );
    void DrawImGui( VkCommandBuffer cmd, VkImageView targetImageView );

    void InitDefaultData( );
    void InitImages( );
    void InitScene( );
    void UpdateScene( ) const;

    void DrawNodeHierarchy( const std::shared_ptr<Node>& node );

    bool                m_isInitialized{ false };
    bool                m_stopRendering{ false };
    VkExtent2D          m_windowExtent{ WIDTH, HEIGHT };
    mutable EngineStats m_stats = { };

    SDL_Window* m_window{ nullptr };

    DeletionQueue m_mainDeletionQueue;

    ImGuiPipeline m_imGuiPipeline = { };

    // post process
    float m_backupGamma = 2.2f;

    ImageId m_whiteImage             = TL::renderer::ImageCodex::InvalidImageId;
    ImageId m_blackImage             = TL::renderer::ImageCodex::InvalidImageId;
    ImageId m_greyImage              = TL::renderer::ImageCodex::InvalidImageId;
    ImageId m_errorCheckerboardImage = TL::renderer::ImageCodex::InvalidImageId;

    std::shared_ptr<Node>                        m_selectedNode = nullptr;
    std::shared_ptr<Camera>                      m_camera;
    std::unique_ptr<FirstPersonFlyingController> m_fpsController;
    CameraController*                            m_cameraController = nullptr;

    TL::world::World m_world;

    bool m_open       = true;
    bool m_drawEditor = true;
    bool m_drawStats  = true;

    float m_timer = 0;
};
