#pragma once

#include <graphics/gbuffer.h>
#include "pipeline.h"

class PbrPipeline : public Pipeline {
public:
    Result<> Init( GfxDevice &gfx ) override;
    void Cleanup( GfxDevice &gfx ) override;
    DrawStats Draw( GfxDevice &gfx, VkCommandBuffer cmd, const GpuSceneData &sceneData, const std::vector<GpuDirectionalLight> &directionalLights, const std::vector<GpuPointLightData> &pointLights, const GBuffer &gBuffer, uint32_t irradianceMap, uint32_t radianceMap, uint32_t brdfLut ) const;

    void DrawDebug( );

private:
    void Reconstruct( GfxDevice &gfx );

    VkDescriptorSetLayout m_ubLayout = nullptr;
    VkDescriptorSet m_set = nullptr;

    struct PushConstants {
        VkDeviceAddress sceneDataAddress;
        uint32_t albedoTex;
        uint32_t normalTex;
        uint32_t positionTex;
        uint32_t pbrTex;
        uint32_t irradianceTex;
        uint32_t radianceTex;
        uint32_t brdfLut;
        uint32_t ssaoTex;
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
            .irradianceFactor = 0.05f,
            .radianceFactor = 0.05f,
            .brdfFactor = 1.0f,
    };
};
