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

#include "compute_pipeline.h"
#include "vk_initializers.h"

void BindlessCompute::AddDescriptorSetLayout( uint32_t binding, VkDescriptorType type ) {
    m_layoutBuilder.AddBinding( binding, type );
}

void BindlessCompute::AddPushConstantRange( uint32_t size ) {
    const VkPushConstantRange range = {
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .offset = 0,
            .size = size,
    };
    m_pushConstantRanges.push_back( range );
}

void BindlessCompute::Build( const GfxDevice &gfx, VkShaderModule shader, const std::string &name ) {
    const auto bindless_layout = gfx.GetBindlessLayout( );

    m_descriptorLayout = m_layoutBuilder.Build( gfx.device, VK_SHADER_STAGE_COMPUTE_BIT );

    const VkDescriptorSetLayout layouts[] = { bindless_layout, m_descriptorLayout };

    VkPipelineLayoutCreateInfo layout_info = vk_init::PipelineLayoutCreateInfo( );
    layout_info.pSetLayouts = layouts;
    layout_info.setLayoutCount = 2;
    layout_info.pPushConstantRanges = m_pushConstantRanges.data( );
    layout_info.pushConstantRangeCount = m_pushConstantRanges.size( );
    VK_CHECK( vkCreatePipelineLayout( gfx.device, &layout_info, nullptr, &m_layout ) );

    const VkPipelineShaderStageCreateInfo stage_create_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = shader,
            .pName = "main",
    };

    VkComputePipelineCreateInfo create_info{ };
    create_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    create_info.pNext = nullptr;
    create_info.layout = m_layout;
    create_info.stage = stage_create_info;

    VK_CHECK( vkCreateComputePipelines( gfx.device, VK_NULL_HANDLE, 1, &create_info, nullptr, &m_pipeline ) );

#ifdef ENABLE_DEBUG_UTILS
    const VkDebugUtilsObjectNameInfoEXT obj = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
            .pNext = nullptr,
            .objectType = VK_OBJECT_TYPE_PIPELINE,
            .objectHandle = reinterpret_cast<uint64_t>( m_pipeline ),
            .pObjectName = name.c_str( ),
    };
    vkSetDebugUtilsObjectNameEXT( gfx.device, &obj );
#endif
}

void BindlessCompute::Cleanup( const GfxDevice &gfx ) const {
    if ( m_pipeline != VK_NULL_HANDLE ) {
        vkDestroyPipeline( gfx.device, m_pipeline, nullptr );
    }

    if ( m_layout != VK_NULL_HANDLE ) {
        vkDestroyPipelineLayout( gfx.device, m_layout, nullptr );
    }

    if ( m_layout != VK_NULL_HANDLE ) {
        vkDestroyDescriptorSetLayout( gfx.device, m_descriptorLayout, nullptr );
    }
}


void BindlessCompute::Bind( VkCommandBuffer cmd ) const {
    vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline );
}

void BindlessCompute::BindDescriptorSet( VkCommandBuffer cmd, VkDescriptorSet set, uint32_t index ) const {
    vkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_layout, index, 1, &set, 0, nullptr );
}

void BindlessCompute::PushConstants( VkCommandBuffer cmd, uint32_t size, const void *value ) const {
    vkCmdPushConstants( cmd, m_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, size, value );
}

void BindlessCompute::Dispatch( VkCommandBuffer cmd, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ ) const {
    vkCmdDispatch( cmd, groupCountX, groupCountY, groupCountZ );
}
