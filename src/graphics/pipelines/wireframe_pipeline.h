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

#include "pipeline.h"

struct MeshDrawCommand;

class WireframePipeline : public Pipeline {
public:
    Result<> Init( TL_VkContext &gfx ) override;
    void Cleanup( TL_VkContext &gfx ) override;
    DrawStats Draw( TL_VkContext &gfx, VkCommandBuffer cmd, const std::vector<MeshDrawCommand> &drawCommands, const GpuSceneData &sceneData ) const;

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
