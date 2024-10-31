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

#include <graphics/descriptors.h>
#include <graphics/gbuffer.h>
#include "graphics/tl_renderer.h"
#include "pipeline.h"

class PbrPipeline : public Pipeline {
public:
    Result<> Init( TL_VkContext &gfx ) override;
    void Cleanup( TL_VkContext &gfx ) override;
    DrawStats Draw( TL_VkContext &gfx, VkCommandBuffer cmd, const GpuSceneData &sceneData, const std::vector<TL::GpuDirectionalLight> &directionalLights, const std::vector<TL::GpuPointLight> &pointLights, const GBuffer &gBuffer, uint32_t irradianceMap, uint32_t radianceMap, uint32_t brdfLut ) const;

    void DrawDebug( );

private:
    void Reconstruct( TL_VkContext &gfx );

    VkDescriptorSetLayout m_ubLayout = nullptr;
    MultiDescriptorSet m_sets;

    struct PushConstants {
        VkDeviceAddress sceneDataAddress;
        uint32_t albedoTex;
        uint32_t normalTex;
        uint32_t positionTex;
        uint32_t pbrTex;
        uint32_t irradianceTex;
        uint32_t radianceTex;
        uint32_t brdfLut;
    };

    struct IblSettings {
        float irradianceFactor;
        float radianceFactor;
        float brdfFactor;
        int padding;
    };

    GpuBuffer m_gpuSceneData = { };
    mutable GpuBuffer m_gpuIbl = { };
    mutable GpuBuffer m_gpuDirectionalLights = { };
    mutable GpuBuffer m_gpuPointLights = { };

    IblSettings m_ibl = {
            .irradianceFactor = 0.3f,
            .radianceFactor = 0.05f,
            .brdfFactor = 1.0f,
    };
};
