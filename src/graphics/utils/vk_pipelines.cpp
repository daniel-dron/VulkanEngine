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

#include <graphics/utils/vk_initializers.h>
#include <graphics/utils/vk_pipelines.h>

void PipelineBuilder::Clear( ) {
    m_inputAssembly = { .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    m_rasterizer = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    m_multisampling = { .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    m_pipelineLayout = { };
    m_depthStencil = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    m_renderInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
    m_shaderStages.clear( );
}

VkPipeline PipelineBuilder::Build( const VkDevice device ) const {
    VkPipelineViewportStateCreateInfo viewport_state = { };
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.pNext = nullptr;

    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;

    VkPipelineColorBlendStateCreateInfo color_blending = { };
    color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blending.pNext = nullptr;

    color_blending.logicOpEnable = VK_FALSE;
    color_blending.logicOp = VK_LOGIC_OP_COPY;
    color_blending.attachmentCount = ( u32 )m_attachmentCount;

    std::vector<VkPipelineColorBlendAttachmentState> blend_attachments;
    blend_attachments.resize( m_attachmentCount, m_colorBlendAttachment );
    if ( m_attachmentCount == 0 ) {
        color_blending.pAttachments = nullptr;
    }
    else {
        color_blending.pAttachments = blend_attachments.data( );
    }

    constexpr VkPipelineVertexInputStateCreateInfo vertex_input_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };

    VkGraphicsPipelineCreateInfo pipeline_info = { .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    pipeline_info.pNext = &m_renderInfo;

    pipeline_info.stageCount = static_cast<uint32_t>( m_shaderStages.size( ) );
    pipeline_info.pStages = m_shaderStages.data( );
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &m_inputAssembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &m_rasterizer;
    pipeline_info.pMultisampleState = &m_multisampling;
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.pDepthStencilState = &m_depthStencil;
    pipeline_info.layout = m_pipelineLayout;

    constexpr VkDynamicState state[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    VkPipelineDynamicStateCreateInfo dynamic_info = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dynamic_info.pDynamicStates = &state[0];
    dynamic_info.dynamicStateCount = 2;

    pipeline_info.pDynamicState = &dynamic_info;

    VkPipeline new_pipeline;
    if ( vkCreateGraphicsPipelines( device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &new_pipeline ) !=
         VK_SUCCESS ) {
        return VK_NULL_HANDLE;
    }

    return new_pipeline;
}

void PipelineBuilder::SetShaders( const VkShaderModule vertexShader, const VkShaderModule fragmentShader ) {
    m_shaderStages.clear( );
    m_shaderStages.push_back( vk_init::PipelineShaderStageCreateInfo( VK_SHADER_STAGE_VERTEX_BIT, vertexShader ) );
    m_shaderStages.push_back( vk_init::PipelineShaderStageCreateInfo( VK_SHADER_STAGE_FRAGMENT_BIT, fragmentShader ) );
}

void PipelineBuilder::SetInputTopology( const VkPrimitiveTopology topology ) {
    m_inputAssembly.topology = topology;
    m_inputAssembly.primitiveRestartEnable = VK_FALSE;
}

void PipelineBuilder::SetPolygonMode( const VkPolygonMode mode ) {
    m_rasterizer.polygonMode = mode;
    m_rasterizer.lineWidth = 1.0f;
}

void PipelineBuilder::SetCullMode( const VkCullModeFlags cullMode, const VkFrontFace frontFace ) {
    m_rasterizer.cullMode = cullMode;
    m_rasterizer.frontFace = frontFace;
}

void PipelineBuilder::SetMultisamplingNone( ) {
    m_multisampling.sampleShadingEnable = VK_FALSE;
    m_multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    m_multisampling.minSampleShading = 1.0f;
    m_multisampling.pSampleMask = nullptr;
    m_multisampling.alphaToCoverageEnable = VK_FALSE;
    m_multisampling.alphaToOneEnable = VK_FALSE;
}

void PipelineBuilder::DisableBlending( ) {
    m_colorBlendAttachment.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    m_colorBlendAttachment.blendEnable = VK_FALSE;
}

void PipelineBuilder::EnableBlendingAdditive( ) {
    m_colorBlendAttachment.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    m_colorBlendAttachment.blendEnable = VK_TRUE;
    m_colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    m_colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
    m_colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    m_colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    m_colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    m_colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
}

void PipelineBuilder::EnableBlendingAlphaBlend( ) {
    m_colorBlendAttachment.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    m_colorBlendAttachment.blendEnable = VK_TRUE;
    m_colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    m_colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    m_colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    m_colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    m_colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    m_colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
}

void PipelineBuilder::EnableBlending( const VkBlendOp blendOp, const VkBlendFactor src, const VkBlendFactor dst,
                                      const VkBlendFactor srcAlpha, const VkBlendFactor dstAlpha ) {
    m_colorBlendAttachment.blendEnable = VK_TRUE;
    m_colorBlendAttachment.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    m_colorBlendAttachment.srcColorBlendFactor = src;
    m_colorBlendAttachment.dstColorBlendFactor = dst;
    m_colorBlendAttachment.colorBlendOp = blendOp;
    m_colorBlendAttachment.srcAlphaBlendFactor = srcAlpha;
    m_colorBlendAttachment.dstAlphaBlendFactor = dstAlpha;
    m_colorBlendAttachment.alphaBlendOp = blendOp;
}

void PipelineBuilder::SetDepthFormat( const VkFormat format ) { m_renderInfo.depthAttachmentFormat = format; }

void PipelineBuilder::SetColorAttachmentFormat( const VkFormat format ) {
    m_colorAttachmentFormat = format;
    m_attachmentCount = 1;
    m_renderInfo.colorAttachmentCount = 1;
    m_renderInfo.pColorAttachmentFormats = &m_colorAttachmentFormat;
}

void PipelineBuilder::SetMultiview( ) { m_renderInfo.viewMask = 0x3f; }

void PipelineBuilder::SetColorAttachmentFormats( const VkFormat *formats, const size_t count ) {
    m_attachmentCount = count;
    m_renderInfo.colorAttachmentCount = ( u32 )count;
    m_renderInfo.pColorAttachmentFormats = formats;
}

void PipelineBuilder::DisableDepthTest( ) {
    m_depthStencil.depthTestEnable = VK_FALSE;
    m_depthStencil.depthWriteEnable = VK_FALSE;
    m_depthStencil.depthCompareOp = VK_COMPARE_OP_NEVER;
    m_depthStencil.depthBoundsTestEnable = VK_FALSE;
    m_depthStencil.stencilTestEnable = VK_FALSE;
    m_depthStencil.front = { };
    m_depthStencil.back = { };
    m_depthStencil.minDepthBounds = 0.f;
    m_depthStencil.maxDepthBounds = 1.f;
}

void PipelineBuilder::EnableDepthTest( const bool depthWriteEnable, const VkCompareOp op ) {
    m_depthStencil.depthTestEnable = VK_TRUE;
    m_depthStencil.depthWriteEnable = depthWriteEnable;
    m_depthStencil.depthCompareOp = op;
    m_depthStencil.depthBoundsTestEnable = VK_FALSE;
    m_depthStencil.stencilTestEnable = VK_FALSE;
    m_depthStencil.front = { };
    m_depthStencil.back = { };
    m_depthStencil.minDepthBounds = 1.f;
    m_depthStencil.maxDepthBounds = 0.f;
}
