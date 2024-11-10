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

#include <graphics/ibl.h>
#include <graphics/resources/r_buffer.h>
#include <graphics/resources/r_resources.h>
#include <graphics/utils/vk_types.h>

#include "draw_command.h"
#include "world/tl_scene.h"

struct Scene;
struct AABoundingBox;

namespace TL {
    struct SceneBuffer {
        Mat4 view;
        Mat4 projection;
        Mat4 viewProjection;
        // Inverse
        Mat4 viewInv;
        Mat4 projectionInv;
        Mat4 viewProjectionInv;

        Vec3 cameraPosition;
        f32  time;
    };

    // TEMP:
    struct GpuSceneData {
        Mat4            view;
        Mat4            proj;
        Mat4            viewproj;
        Mat4            lightProj;
        Mat4            lightView;
        Vec4            fogColor;
        Vec3            cameraPosition;
        float           ambientLightFactor;
        Vec3            ambientLightColor;
        float           fogEnd;
        float           fogStart;
        VkDeviceAddress materials;
        int             numberOfDirectionalLights = 0;
        int             numberOfPointLights       = 0;
    };

    struct MeshPushConstants {
        Mat4            worldFromLocal;
        VkDeviceAddress sceneDataAddress;
        VkDeviceAddress vertexBufferAddress;
        uint32_t        materialId;
    };

    // Indirect stuff
    struct IndirectPushConstant {
        VkDeviceAddress SceneDataAddress;
        VkDeviceAddress PerDrawDataAddress;
    };

    struct PerDrawData {
        Mat4            WorldFromLocal;
        VkDeviceAddress VertexBufferAddress;
        u32             MaterialId;
    };

    struct ShadowMapPushConstants {
        Mat4            projection;
        Mat4            view;
        Mat4            model;
        VkDeviceAddress vertexBufferAddress;
    };

    struct PostProcessPushConstants {
        ImageId hdr;
        ImageId output;
        float   gamma    = 2.2f;
        float   exposure = 1.0f;
    };

    struct PbrPushConstants {
        VkDeviceAddress sceneDataAddress;
        uint32_t        albedoTex;
        uint32_t        normalTex;
        uint32_t        positionTex;
        uint32_t        pbrTex;
        uint32_t        irradianceTex;
        uint32_t        radianceTex;
        uint32_t        brdfLut;
    };

    enum class DebugRenderTarget : u32 {
        ALBEDO,
        NORMAL,
        PBR_FACTORS,
    };

    struct DebugPushConstants {
        VkDeviceAddress   SceneDataAddress;
        uint32_t          AlbedoTex;
        uint32_t          NormalTex;
        uint32_t          PositionTex;
        uint32_t          PbrTex;
        DebugRenderTarget RenderTarget = DebugRenderTarget::NORMAL;
    };

    struct IblSettings {
        float irradianceFactor;
        float radianceFactor;
        float brdfFactor;
        int   padding;
    };

    struct GpuPointLight {
        Vec3  position;
        float constant;
        Vec3  color;
        float linear;
        float quadratic;
        float pad1;
        float pad2;
        float pad3;
    };

    struct GpuDirectionalLight {
        Vec3    direction;
        int     pad1;
        Vec4    color;
        Mat4    proj;
        Mat4    view;
        ImageId shadowMap;
        int     pad2;
        int     pad3;
        int     pad4;
    };

    struct SkyboxPushConstants {
        VkDeviceAddress sceneDataAddress;
        VkDeviceAddress vertexBufferAddress;
        uint32_t        textureId;
    };

    struct RendererOptions {
        bool wireframe                     = false;
        bool vsync                         = true;
        bool renderIrradianceInsteadSkybox = false;

        bool reRenderShadowMaps = true; // whether to rerender the shadow map every frame.

        // Frustum settings
        bool    frustumCulling   = true;  // Turn on/off the entire culling system checks
        bool    useFrozenFrustum = false; // Toggle the usage of the last saved frustum
        Frustum lastSavedFrustum = { };   // The frustum to be used when frustum checks are frozen.
                                          // Used for debug purposes

        DebugRenderTarget RenderTarget = DebugRenderTarget::NORMAL;

        bool UseIndirectDraw = true;
    };

    struct Renderable {
        renderer::MeshHandle     MeshHandle;
        renderer::MaterialHandle MaterialHandle;
        Mat4                     Transform;
        renderer::AABB           Aabb;
        u32                      FirstIndex;
    };

    class Renderer {
    public:
        void Init( struct SDL_Window* window, Vec2 extent );
        void Shutdown( );

        // This will setup the current frame.
        // Will wait for the frame rendering fence, free its arena/resources, acquire the next image
        // in the swapchain and prepare the command buffer to record commands. In a multi in-flight frame
        // setup, this won't wait for the last frame to finish rendering, but instead, it will wait for
        // the desired next frame to finish its previous work. In most cases, this wait time is close to 0,
        // since the CPU was doing a lot of work in parallel with the gpu, which is busy rendering the last frame.
        // Returns the index of the swapchain image.
        void StartFrame( ) noexcept;

        void Frame( ) noexcept;

        // Will queue up the work agreggated in the frame command buffer. This function is non blocking,
        // as the only thing it does is queue up the work to the right graphics queue.
        // It will also transition the swapchain image to its expected layout to be presented in the future.
        void EndFrame( ) noexcept;

        // Will present the swapchain image to the screen.
        void Present( ) noexcept;

        // Upload scene to the rendere
        void UpdateScene( const Scene& scene );
        void UpdateWorld( const world::World& world );

        void OnResize( u32 width, u32 height );

        // Returns the main camera used to render the scene
        std::shared_ptr<Camera> GetCamera( ) { return m_camera; }

        u32                      swapchainImageIndex = -1;
        PostProcessPushConstants postProcessSettings = { };
        IblSettings              iblSettings         = {
                                     .irradianceFactor = 0.3f,
                                     .radianceFactor   = 0.05f,
                                     .brdfFactor       = 1.0f,
        };

        RendererOptions settings;

        static constexpr VkFormat DepthFormat           = VK_FORMAT_D32_SFLOAT;
        static constexpr u8       MaxColorRenderTargets = 8;

    private:
        // This will be called after submitting a frame to the gpu, when its legal to start working on the next one
        void OnFrameBoundary( ) noexcept;
        void AdvanceFrameDependantBuffers( ) const;

        void SetViewportAndScissor( VkCommandBuffer cmd ) noexcept;

        void PreparePbrPass( );
        void PrepareSkyboxPass( );

        void GBufferPass( );
        void IndirectGBufferPass( );
        void DebugPass( );
        void ShadowMapPass( );
        void PbrPass( );
        void SkyboxPass( );
        void PostProcessPass( );

        Vec2 m_extent = { };

        // Main camera
        std::shared_ptr<Camera> m_camera;

        // Scene
        std::vector<Renderable>          m_renderables;
        std::unique_ptr<Ibl>             m_ibl       = nullptr;
        GpuSceneData                     m_sceneData = { };
        std::shared_ptr<Buffer>          m_sceneBufferGpu;
        std::vector<GpuDirectionalLight> m_directionalLights;
        std::vector<GpuPointLight>       m_pointLights;

        // Draw Commands
        VisibilityResult             VisibilityCheckWithLOD( const Mat4& transform, const renderer::AABB* aabb, const Frustum& frustum ) const;
        void                         CreateDrawCommands( );
        std::vector<MeshDrawCommand> m_drawCommands;
        std::vector<MeshDrawCommand> m_shadowMapCommands;

        // Indirect Draw Commands
        void                    PrepareIndirectBuffers( );
        void                    UpdateIndirectCommands( ); // Used to update the indirect draw command buffers if using indirect draw.
        const u64               MAX_DRAWS = 10000;         // Max number of indirect draw calls.
        std::unique_ptr<Buffer> m_indirectBuffer;          // Buffer containing all indexd indirect commands.
        std::unique_ptr<Buffer> m_perDrawDataBuffer;       // List of per instance data used in indirect draw.
        u64                     m_indirectDrawCount     = { };
        Buffer*                 m_currentFrameIndexBlob = nullptr;
        std::vector<u32>        m_firstIndices          = { };

        // PBR pipelin stuff (WIP)
        VkDescriptorSetLayout   m_pbrSetLayout               = VK_NULL_HANDLE;
        MultiDescriptorSet      m_pbrSet                     = { };
        std::shared_ptr<Buffer> m_gpuIbl                     = nullptr;
        std::shared_ptr<Buffer> m_gpuDirectionalLightsBuffer = nullptr;
        std::shared_ptr<Buffer> m_gpuPointLightsBuffer       = nullptr;

        // Skybox
        renderer::MeshHandle m_skyboxMesh = { };
    };
} // namespace TL