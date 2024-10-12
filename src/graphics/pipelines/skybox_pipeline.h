#pragma once

#include "pipeline.h"

class SkyboxPipeline : public Pipeline {
public:
    Result<> Init( GfxDevice &gfx ) override;
    void Cleanup( GfxDevice &gfx ) override;

    void Draw( GfxDevice &gfx, VkCommandBuffer cmd, ImageId skyboxTexture, const GpuSceneData &sceneData ) const;

private:
    struct PushConstants {
        VkDeviceAddress sceneDataAddress;
        VkDeviceAddress vertexBufferAddress;
        uint32_t textureId;
    };

    void CreateCubeMesh( GfxDevice &gfx );
    void Reconstruct( GfxDevice &gfx );

    MeshId m_cubeMesh = 0;
    GpuBuffer m_gpuSceneData = { };
};
