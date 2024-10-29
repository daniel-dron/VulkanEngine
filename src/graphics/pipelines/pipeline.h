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

#include <expected>
#include <vk_types.h>

class TL_VkContext;

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
