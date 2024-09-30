#pragma once

#include <vk_types.h>
#include <graphics/gfx_device.h>
#include <vk_initializers.h>

class BindlessCompute {
public:
	void addDescriptorSetLayout( uint32_t binding, VkDescriptorType type );
	void addPushConstantRange( uint32_t size );
	void build( GfxDevice& gfx, VkShaderModule shader, const std::string& name );

	VkDescriptorSetLayout GetLayout( ) const { return descriptor_layout; };

	void bind( VkCommandBuffer cmd ) const;
	void bindDescriptorSet( VkCommandBuffer cmd, VkDescriptorSet set, uint32_t index ) const;
	void pushConstants( VkCommandBuffer cmd, uint32_t size, const void* value ) const;
	void dispatch( VkCommandBuffer cmd, uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z ) const;

	void cleanup( GfxDevice& gfx );
private:

	VkPipelineLayout layout = VK_NULL_HANDLE;
	VkPipeline pipeline = VK_NULL_HANDLE;

	VkDescriptorSetLayout descriptor_layout;
	std::vector<VkPushConstantRange> push_constant_ranges;
	DescriptorLayoutBuilder layout_builder;
};
