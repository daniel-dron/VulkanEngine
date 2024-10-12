#pragma once

#include <graphics/gfx_device.h>
#include <vk_types.h>

class BindlessCompute {
public:
    void AddDescriptorSetLayout( uint32_t binding, VkDescriptorType type );
    void AddPushConstantRange( uint32_t size );
    void Build( const GfxDevice &gfx, VkShaderModule shader, const std::string &name );

    VkDescriptorSetLayout GetLayout( ) const { return m_descriptorLayout; };

    void Bind( VkCommandBuffer cmd ) const;
    void BindDescriptorSet( VkCommandBuffer cmd, VkDescriptorSet set, uint32_t index ) const;
    void PushConstants( VkCommandBuffer cmd, uint32_t size, const void *value ) const;
    void Dispatch( VkCommandBuffer cmd, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ ) const;

    void Cleanup( const GfxDevice &gfx ) const;

private:
    VkPipelineLayout m_layout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;

    VkDescriptorSetLayout m_descriptorLayout = nullptr;
    std::vector<VkPushConstantRange> m_pushConstantRanges;
    DescriptorLayoutBuilder m_layoutBuilder;
};
