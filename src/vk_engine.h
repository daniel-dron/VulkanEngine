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
#include <imgui_impl_sdl2.h>
#include <utils/ImGuiProfilerRenderer.h>
#include <utils/imgui_console.h>
#include <vk_types.h>
#include <vulkan/vulkan_core.h>
#include "camera/camera.h"
#include "graphics/tl_renderer.h"
#include "graphics/tl_vkcontext.h"
#include "utils/profiler.h"

class TL_Engine;

extern TL_Engine* g_TL;
extern utils::VisualProfiler g_visualProfiler;

struct RendererOptions {
    bool wireframe = false;
    bool vsync = true;
    bool renderIrradianceInsteadSkybox = false;

    bool reRenderShadowMaps     = true;    // whether to rerender the shadow map every frame.

    // Frustum settings
    bool frustumCulling         = true;     // Turn on/off the entire culling system checks
    bool useFrozenFrustum       = false;    // Toggle the usage of the last saved frustum   
    Frustum lastSavedFrustum    = { };      // The frustum to be used when frustum checks are frozen.
                                            // Used for debug purposes

    // LODs
    bool lodSystem              = true;     // Toggle LOD system usage
    bool freezeLodSystem        = false;    // Will save the currently used LOD for each node in the scene
                                            // and use those instead.
};

class TL_Engine {
public:
    static TL_Engine &Get( );
    static TL_VkContext &VkContext( );

    void Init( );
    void Cleanup( );
    void Draw( );
    void Run( );

    void ResizeSwapchain( uint32_t width, uint32_t height );

    ImGuiConsole console;
    std::unique_ptr<TL::Renderer> renderer = nullptr;

private:
    void InitSdl( );
    void InitRenderer( );

    void InitImGui( );
    void DrawImGui( VkCommandBuffer cmd, VkImageView targetImageView );

    void CreateDrawCommands( TL_VkContext &gfx, const Scene &scene, Node &node );
    VisibilityLODResult VisibilityCheckWithLOD( const Mat4 &transform, const AABoundingBox *aabb,
                                                const Frustum &frustum );

    void GBufferPass( VkCommandBuffer cmd );
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
    mutable EngineStats m_stats = { };

    SDL_Window *m_window{ nullptr };

    bool m_dirtSwapchain = false;

    DeletionQueue m_mainDeletionQueue;

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
    MultiDescriptorSet m_postProcessSet;

    // blur
    struct BlurSettings {
        int sourceTex;
        int size;
    };
    mutable BlurSettings m_blurSettings = { };
    BindlessCompute m_blurPipeline = { };
    MultiDescriptorSet m_blurSet;

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
    std::vector<MeshDrawCommand> m_shadowMapCommands;

    std::shared_ptr<Node> m_selectedNode = nullptr;
    std::unique_ptr<Scene> m_scene;
    std::unique_ptr<Camera> m_camera;
    std::unique_ptr<FirstPersonFlyingController> m_fpsController;
    CameraController *m_cameraController = nullptr;

    RendererOptions m_rendererOptions;

    bool m_open = true;
    bool m_drawEditor = true;
    bool m_drawStats = true;

    float m_timer = 0;
};
