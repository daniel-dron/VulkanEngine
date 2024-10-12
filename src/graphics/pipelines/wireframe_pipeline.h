#pragma once

#include "pipeline.h"

struct MeshDrawCommand;

class WireframePipeline : public Pipeline {
public:
    Result<> Init( GfxDevice &gfx ) override;
    void Cleanup( GfxDevice &gfx ) override;
    DrawStats Draw( GfxDevice &gfx, VkCommandBuffer cmd, const std::vector<MeshDrawCommand> &drawCommands, const GpuSceneData &sceneData ) const;

    VkPipeline GetPipeline( ) const { return this->m_pipeline; }

private:
    struct PushConstants {
        Mat4 worldFromLocal;
        VkDeviceAddress sceneDataAddress;
        VkDeviceAddress vertexBufferAddress;
        uint32_t materialId;
    };

    GpuBuffer m_gpuSceneData = { };
};
