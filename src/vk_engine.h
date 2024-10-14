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
#include <graphics/draw_command.h>
#include <graphics/ibl.h>
#include <graphics/material_codex.h>
#include <graphics/pipelines/gbuffer_pipeline.h>
#include <graphics/pipelines/imgui_pipeline.h>
#include <graphics/pipelines/pbr_pipeline.h>
#include <graphics/pipelines/shadowmap.h>
#include <graphics/pipelines/skybox_pipeline.h>
#include <graphics/pipelines/wireframe_pipeline.h>
#include <utils/ImGuiProfilerRenderer.h>
#include <vk_types.h>
#include <vulkan/vulkan_core.h>
#include "camera/camera.h"
#include "graphics/gfx_device.h"
#include "utils/profiler.h"
#include <utils/imgui_console.h>

class VulkanEngine;

struct RendererOptions {
    bool wireframe = false;
    bool frustum = false;
    bool vsync = true;
    bool renderIrradianceInsteadSkybox = false;
};

class VulkanEngine {
public:
    static VulkanEngine &Get( );

    void Init( );
    void Cleanup( );
    void Draw( );
    void Run( );

    void ResizeSwapchain( uint32_t width, uint32_t height );
    
    ImGuiConsole console;

private:
    void InitSdl( );
    void InitVulkan( );

    void InitImGui( );
    void DrawImGui( VkCommandBuffer cmd, VkImageView targetImageView );

    void ConstructSsaoPipeline( );
    void ActuallyConstructSsaoPipeline( );

    void ConstructBlurPipeline( );
    void ActuallyConstructBlurPipeline( );

    void GBufferPass( VkCommandBuffer cmd ) const;
    void SsaoPass( VkCommandBuffer cmd ) const;
    void PbrPass( VkCommandBuffer cmd ) const;
    void SkyboxPass( VkCommandBuffer cmd ) const;
    void PostProcessPass( VkCommandBuffer cmd ) const;
    void ShadowMapPass( VkCommandBuffer cmd ) const;

    void InitDefaultData( );
    void InitImages( );
    void InitScene( );
    void UpdateScene( );

    void DrawNodeHierarchy( const std::shared_ptr<Node> &node );

private:
    bool m_isInitialized{ false };
    bool m_stopRendering{ false };
    VkExtent2D m_windowExtent{ WIDTH, HEIGHT };
    EngineStats m_stats = { };

    SDL_Window *m_window{ nullptr };

    std::unique_ptr<GfxDevice> m_gfx;
    bool m_dirtSwapchain = false;

    DeletionQueue m_mainDeletionQueue;
    VkExtent2D m_drawExtent = { };
    float m_renderScale = 1.f;

    MaterialCodex m_materialCodex = { };
    PbrPipeline m_pbrPipeline = { };
    WireframePipeline m_wireframePipeline = { };
    GBufferPipeline m_gBufferPipeline = { };
    ImGuiPipeline m_imGuiPipeline = { };
    SkyboxPipeline m_skyboxPipeline = { };
    ShadowMap m_shadowMapPipeline = { };

    // post process
    struct PostProcessConfig {
        ImageId hdr;
        float gamma = 2.2f;
        float exposure = 1.0f;
    };
    float m_backupGamma = 2.2f;
    mutable PostProcessConfig m_ppConfig = { };
    BindlessCompute m_postProcessPipeline = { };
    VkDescriptorSet m_postProcessSet = nullptr;

    // ssao
    struct SsaoSettings {
        bool enable = false;
        int kernelSize = 32;
        float radius = 0.5;
        float bias = 0.025f;
        int blurSize = 2;
        float power = 2;
        int noiseTexture;
        int depthTexture;
        int normalTexture;
        VkDeviceAddress scene;
    };
    mutable SsaoSettings m_ssaoSettings = { };
    BindlessCompute m_ssaoPipeline = { };
    VkDescriptorSet m_ssaoSet = nullptr;
    GpuBuffer m_ssaoBuffer = { };
    GpuBuffer m_ssaoKernel = { };

    // blur
    struct BlurSettings {
        int sourceTex;
        int size;
    };
    mutable BlurSettings m_blurSettings = { };
    BindlessCompute m_blurPipeline = { };
    VkDescriptorSet m_blurSet = nullptr;

    // ----------
    // scene
    std::vector<GpuDirectionalLight> m_gpuDirectionalLights;
    std::vector<GpuPointLightData> m_gpuPointLights;
    GpuSceneData m_sceneData = { };
    GpuBuffer m_gpuSceneData = { };

    ImageId m_whiteImage = ImageCodex::InvalidImageId;
    ImageId m_blackImage = ImageCodex::InvalidImageId;
    ImageId m_greyImage = ImageCodex::InvalidImageId;
    ImageId m_errorCheckerboardImage = ImageCodex::InvalidImageId;

    Ibl m_ibl = { };

    std::vector<MeshDrawCommand> m_drawCommands;

    std::shared_ptr<Node> m_selectedNode = nullptr;
    std::unique_ptr<Scene> m_scene;
    std::unique_ptr<Camera> m_camera;
    std::unique_ptr<FirstPersonFlyingController> m_fpsController;
    CameraController *m_cameraController = nullptr;

    utils::VisualProfiler m_visualProfiler = utils::VisualProfiler( 300 );
    RendererOptions m_rendererOptions;

    bool m_open = true;
    bool m_drawEditor = true;
    bool m_drawStats = true;
    
    float m_timer = 0;
};
