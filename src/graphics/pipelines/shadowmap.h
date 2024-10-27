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

class ShadowMap : public Pipeline {
public:
    Result<> Init( TL_VkContext &gfx ) override;
    void Cleanup( TL_VkContext &gfx ) override;

    DrawStats Draw( TL_VkContext &gfx, VkCommandBuffer cmd, const std::vector<MeshDrawCommand> &drawCommands,
                    const std::vector<GpuDirectionalLight> &lights ) const;

private:
    struct PushConstants {
        Mat4 projection;
        Mat4 view;
        Mat4 model;
        VkDeviceAddress vertexBufferAddress;
    };

    void Reconstruct( TL_VkContext &gfx );
};
