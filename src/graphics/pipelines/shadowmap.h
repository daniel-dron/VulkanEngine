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
    Result<> Init( GfxDevice &gfx ) override;
    void Cleanup( GfxDevice &gfx ) override;

    DrawStats Draw( GfxDevice &gfx, VkCommandBuffer cmd, const std::vector<MeshDrawCommand> &drawCommands, const glm::mat4 &projection, const glm::mat4 &view, ImageId target ) const;

private:
    struct PushConstants {
        Mat4 projection;
        Mat4 view;
        Mat4 model;
        VkDeviceAddress vertexBufferAddress;
    };

    void Reconstruct( GfxDevice &gfx );
};
