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

class SkyboxPipeline : public Pipeline {
public:
    Result<> Init( TL_VkContext &gfx ) override;
    void Cleanup( TL_VkContext &gfx ) override;

    void Draw( TL_VkContext &gfx, VkCommandBuffer cmd, ImageId skyboxTexture, const GpuSceneData &sceneData ) const;

private:
    struct PushConstants {
        VkDeviceAddress sceneDataAddress;
        VkDeviceAddress vertexBufferAddress;
        uint32_t textureId;
    };

    void CreateCubeMesh( TL_VkContext &gfx );
    void Reconstruct( TL_VkContext &gfx );

    MeshId m_cubeMesh = 0;
    GpuBuffer m_gpuSceneData = { };
};
