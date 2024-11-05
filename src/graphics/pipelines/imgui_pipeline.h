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

class TL_VkContext;

// TODO: Refactor this leftover code
class Pipeline {
public:
    enum class Error { ShaderLoadingFailed };

    struct PipelineError {
        Error error;
        std::string message;
    };

    template<typename T = void>
    using Result = std::expected<T, PipelineError>;

    virtual Result<> Init( TL_VkContext &gfx ) = 0;
    virtual void Cleanup( TL_VkContext &gfx ) = 0;

protected:
    VkPipeline m_pipeline = nullptr;
    VkPipelineLayout m_layout = nullptr;
};

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
