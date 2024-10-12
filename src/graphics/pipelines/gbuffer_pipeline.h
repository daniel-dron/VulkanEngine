#pragma once

#include "pipeline.h"

struct MeshDrawCommand;

class GBufferPipeline : public Pipeline {
public:
    Result<> Init( GfxDevice &gfx ) override;
    void Cleanup( GfxDevice &gfx ) override;
    DrawStats Draw( GfxDevice &gfx, VkCommandBuffer cmd, const std::vector<MeshDrawCommand> &drawCommands, const GpuSceneData &sceneData ) const;

private:
    struct PushConstants {
        Mat4 worldFromLocal;
        VkDeviceAddress sceneDataAddress;
        VkDeviceAddress vertexBufferAddress;
        uint32_t materialId;
    };

    void Reconstruct( GfxDevice &gfx );

    GpuBuffer m_gpuSceneData = { };
};
