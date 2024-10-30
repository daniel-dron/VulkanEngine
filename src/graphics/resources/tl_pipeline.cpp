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

#include <pch.h>

#include "tl_pipeline.h"

namespace TL {
    static VkPipelineColorBlendAttachmentState
    CreateBlendAttachmentState( const PipelineConfig::BlendType &blendType ) {
        if ( blendType == PipelineConfig::BlendType::ADDITIVE ) {
            return {
                    .blendEnable = VK_TRUE,
                    .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
                    .dstColorBlendFactor = VK_BLEND_FACTOR_ONE,
                    .colorBlendOp = VK_BLEND_OP_ADD,
                    .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                    .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
                    .alphaBlendOp = VK_BLEND_OP_ADD,
                    .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                            VK_COLOR_COMPONENT_A_BIT,
            };
        }

        if ( blendType == PipelineConfig::BlendType::ALPHA_BLEND ) {
            return {
                    .blendEnable = VK_TRUE,
                    .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
                    .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                    .colorBlendOp = VK_BLEND_OP_ADD,
                    .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                    .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
                    .alphaBlendOp = VK_BLEND_OP_ADD,
                    .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                            VK_COLOR_COMPONENT_A_BIT,
            };
        }

        if ( blendType == PipelineConfig::BlendType::OFF ) {
            return {
                    .blendEnable = VK_FALSE,
                    .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                            VK_COLOR_COMPONENT_A_BIT,
            };
        }

        std::unreachable( );
    }

    Pipeline::Pipeline( const PipelineConfig &config ) {
        // shader stage
        const auto vertex = shaders::LoadShaderModule( vkctx->device, config.vertex );
        const auto pixel = shaders::LoadShaderModule( vkctx->device, config.pixel );

        assert( vertex != VK_NULL_HANDLE && "Could not find vertex shader" );
        assert( pixel != VK_NULL_HANDLE && "Could not find pixel shader" );

        std::array shader_stages_info = {
                VkPipelineShaderStageCreateInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                                 .pNext = nullptr,
                                                 .flags = 0,
                                                 .stage = VK_SHADER_STAGE_VERTEX_BIT,
                                                 .module = vertex,
                                                 .pName = "main",
                                                 .pSpecializationInfo = nullptr },
                VkPipelineShaderStageCreateInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                                 .pNext = nullptr,
                                                 .flags = 0,
                                                 .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                                                 .module = pixel,
                                                 .pName = "main",
                                                 .pSpecializationInfo = nullptr } };

        std::vector<VkFormat> color_formats;
        std::vector<VkPipelineColorBlendAttachmentState> blend_attachments;
        color_formats.reserve( config.colorTargets.size( ) );
        blend_attachments.reserve( config.colorTargets.size( ) );
        for ( auto &color_targets_config : config.colorTargets ) {
            color_formats.push_back( color_targets_config.format );
            blend_attachments.push_back( CreateBlendAttachmentState( color_targets_config.blendType ) );
        }

        // We do not make use of render passes, thus we need to create a Rendering Info structure
        VkPipelineRenderingCreateInfo render_info = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                                                      .pNext = nullptr,

                                                      .colorAttachmentCount =
                                                              static_cast<uint32_t>( color_formats.size( ) ),
                                                      .pColorAttachmentFormats = color_formats.data( ),

                                                      .depthAttachmentFormat = Renderer::DepthFormat };

        // No need to configribe viewports and scissor state here since we will be using dynamic states
        VkPipelineViewportStateCreateInfo viewport_info = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
                .pNext = nullptr,
                .viewportCount = 1,
                .scissorCount = 1 };


        VkPipelineColorBlendStateCreateInfo blend_info = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                .pNext = nullptr,
                .logicOpEnable = VK_FALSE,
                .attachmentCount = static_cast<uint32_t>( blend_attachments.size( ) ),
                .pAttachments = blend_attachments.size( ) == 0 ? nullptr : blend_attachments.data( ) };

        // This project used device address for vertex buffers, so we don't really need to configribe anything for
        // vertex input stage
        VkPipelineVertexInputStateCreateInfo vertex_info = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };

        // We use dynamic state for scissor and viewport. This, together with dynamic rendering, makes it really nice
        // and simple to use modern vulkan
        std::array<VkDynamicState, 2> state = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dynamic_info = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                                                          .pNext = nullptr,
                                                          .dynamicStateCount = static_cast<uint32_t>( state.size( ) ),
                                                          .pDynamicStates = state.data( ) };

        VkPipelineInputAssemblyStateCreateInfo input_info = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                .pNext = nullptr,
                .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST };

        VkPipelineRasterizationStateCreateInfo raster_info = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                .pNext = nullptr,

                .polygonMode = config.polygonMode,
                .cullMode = config.cullMode,
                .frontFace = VK_FRONT_FACE_CLOCKWISE,
                .lineWidth = config.lineWidth };

        // TODO: expose this through the parameter structure
        VkPipelineMultisampleStateCreateInfo multisample_info = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                .pNext = nullptr,
                .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
                .sampleShadingEnable = VK_FALSE,
                .minSampleShading = 1.0f,
                .pSampleMask = nullptr,
                .alphaToCoverageEnable = VK_FALSE,
                .alphaToOneEnable = VK_FALSE };

        VkPipelineDepthStencilStateCreateInfo depth_stencil_info = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
                .pNext = nullptr,
                .depthTestEnable = config.depthTest,
                .depthWriteEnable = config.depthWrite,
                .depthCompareOp = config.depthCompare,
                .stencilTestEnable = false,
                .front = { },
                .back = { },
                .minDepthBounds = 1.0f,
                .maxDepthBounds = 0.0f };

        VkPipelineLayoutCreateInfo layout_info = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .setLayoutCount = static_cast<uint32_t>( config.descriptorSetLayouts.size( ) ),
                .pSetLayouts = config.descriptorSetLayouts.empty( ) ? nullptr : config.descriptorSetLayouts.data( ),

                .pushConstantRangeCount = static_cast<uint32_t>( config.pushConstantRanges.size( ) ),
                .pPushConstantRanges =
                        config.pushConstantRanges.empty( ) ? nullptr : config.pushConstantRanges.data( ) };

        VKCALL( vkCreatePipelineLayout( vkctx->device, &layout_info, nullptr, &layout ) );

        VkGraphicsPipelineCreateInfo pipeline_info = { .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                                                       .pNext = &render_info,

                                                       .stageCount =
                                                               static_cast<uint32_t>( shader_stages_info.size( ) ),
                                                       .pStages = shader_stages_info.data( ),
                                                       .pVertexInputState = &vertex_info,
                                                       .pInputAssemblyState = &input_info,
                                                       .pViewportState = &viewport_info,
                                                       .pRasterizationState = &raster_info,
                                                       .pMultisampleState = &multisample_info,
                                                       .pDepthStencilState = &depth_stencil_info,
                                                       .pColorBlendState = &blend_info,
                                                       .pDynamicState = &dynamic_info,
                                                       .layout = layout };

        if ( vkCreateGraphicsPipelines( vkctx->device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline ) !=
             VK_SUCCESS ) {
            assert( false && "Failed to create pipeline" );
        }

        vkctx->SetObjectDebugName( VK_OBJECT_TYPE_PIPELINE, pipeline, config.name );
        vkctx->SetObjectDebugName( VK_OBJECT_TYPE_PIPELINE_LAYOUT, layout, fmt::format( "{} Layout", config.name ) );

        // We no longer need the shader modules
        vkDestroyShaderModule( vkctx->device, vertex, nullptr );
        vkDestroyShaderModule( vkctx->device, pixel, nullptr );
    }

    Pipeline::~Pipeline( ) {
        vkDestroyPipelineLayout( vkctx->device, layout, nullptr );
        vkDestroyPipeline( vkctx->device, pipeline, nullptr );
    }
} // namespace TL