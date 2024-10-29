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

#include <graphics/tl_renderer.h>

namespace TL {
    struct PipelineConfig {
        std::string vertex;
        std::string pixel;

        VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL;
        float lineWidth = 1.0f;

        bool depthTest = true;
        bool depthWrite = true;
        VkCompareOp depthCompare = VK_COMPARE_OP_LESS_OR_EQUAL;

        enum class BlendType { OFF, ADDITIVE, ALPHA_BLEND };
        struct ColorTargetsConfig {
            VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;
            BlendType blendType = BlendType::OFF;
        };
        std::vector<ColorTargetsConfig> colorTargets;

        // TODO: In the future, would be nice to extrapolate the layout from the vertex & pixel shaders directly
        // For now, this will do...
        std::vector<VkPushConstantRange> pushConstantRanges;
        std::vector<VkDescriptorSetLayout> descriptorSetLayouts;

        // This value is only for debugging purposes only, and has no other side effect
        std::string debugName;
    };

    class Pipeline {
    public:
        explicit Pipeline( TL_VkContext &ctx, const PipelineConfig &config );
        ~Pipeline( );

    private:
        VkPipeline pipeline = VK_NULL_HANDLE;
        VkPipelineLayout layout = VK_NULL_HANDLE;

        TL_VkContext &m_ctx;
    };

} // namespace TL
