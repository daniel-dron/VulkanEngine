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

#include <graphics/resources/tl_buffer.h>
#include <graphics/resources/tl_pipeline.h>
#include <graphics/tl_vkcontext.h>
#include <vk_types.h>

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
        int             numberOfDirectionalLights;
        int             numberOfPointLights;
    };

    // TODO: move this
    struct MeshPushConstants {
        Mat4            worldFromLocal;
        VkDeviceAddress sceneDataAddress;
        VkDeviceAddress vertexBufferAddress;
        uint32_t        materialId;
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
        uint32_t textureId;
    };

    class Renderer {
    public:
        void Init( struct SDL_Window *window, Vec2 extent );
        void Cleanup( );

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
        void UpdateScene( const Scene &scene );

        // Returns the main camera used to render the scene
        std::shared_ptr<Camera> GetCamera( ) { return m_camera; }

        u32                      swapchainImageIndex = -1;
        PostProcessPushConstants postProcessSettings = { };
        IblSettings              iblSettings         = {
                                     .irradianceFactor = 0.3f,
                                     .radianceFactor   = 0.05f,
                                     .brdfFactor       = 1.0f,
        };

        static constexpr VkFormat DepthFormat           = VK_FORMAT_D32_SFLOAT;
        static constexpr u8       MaxColorRenderTargets = 8;

    private:
        // This will be called after submitting a frame to the gpu, when its legal to start working on the next one
        void OnFrameBoundary( ) noexcept;

        void SetViewportAndScissor( VkCommandBuffer cmd ) noexcept;

        void PreparePbrPass( );
        void PrepareSkyboxPass( );

        void GBufferPass( );
        void ShadowMapPass( );
        void PbrPass( );
        void SkyboxPass( );
        void PostProcessPass( );

        Vec2 m_extent = { };

        // Main camera
        std::shared_ptr<Camera>          m_camera;
        std::shared_ptr<Buffer>          m_sceneBufferGpu;
        std::vector<GpuDirectionalLight> m_directionalLights;
        std::vector<GpuPointLight>       m_pointLights;

        // PBR pipelin stuff (WIP)
        VkDescriptorSetLayout   m_pbrSetLayout               = VK_NULL_HANDLE;
        MultiDescriptorSet      m_pbrSet                     = { };
        std::shared_ptr<Buffer> m_gpuIbl                     = nullptr;
        std::shared_ptr<Buffer> m_gpuDirectionalLightsBuffer = nullptr;
        std::shared_ptr<Buffer> m_gpuPointLightsBuffer       = nullptr;

        // Skybox
        MeshId m_skyboxMesh = -1;
    };
} // namespace TL