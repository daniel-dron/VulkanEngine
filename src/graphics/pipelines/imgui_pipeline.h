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

#include <imgui.h>
#include "pipeline.h"

class ImGuiPipeline : public Pipeline {
public:
    Result<> Init( TL_VkContext &gfx ) override;
    void     Cleanup( TL_VkContext &gfx ) override;

    void Draw( TL_VkContext &gfx, VkCommandBuffer cmd, ImDrawData *drawData );

private:
    struct PushConstants {
        VkDeviceAddress vertexBuffer;
        uint32_t        textureId;
        uint32_t        isSrgb;
        Vec2            offset;
        Vec2            scale;
    };

    ImTextureID                 m_fontTextureId = 0;
    std::shared_ptr<TL::Buffer> m_indexBuffer;
    std::shared_ptr<TL::Buffer> m_vertexBuffer;
};
