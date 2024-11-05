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

#include <vulkan/vulkan_core.h>

class PipelineBuilder {
public:
	PipelineBuilder( ) { Clear( ); }

	void Clear( );
	void SetShaders( VkShaderModule vertexShader, VkShaderModule fragmentShader );
	void SetInputTopology( VkPrimitiveTopology topology );
	void SetPolygonMode( VkPolygonMode mode );
	void SetCullMode( VkCullModeFlags cullMode, VkFrontFace frontFace );
	void SetMultisamplingNone( );
	void DisableBlending( );
	void EnableBlendingAlphaBlend();
	void EnableBlending( VkBlendOp blendOp, VkBlendFactor src, VkBlendFactor dst, VkBlendFactor srcAlpha, VkBlendFactor dstAlpha );
	void EnableBlendingAdditive( );
	void SetDepthFormat( VkFormat format );
	void SetColorAttachmentFormat( VkFormat format );
	void SetMultiview();
	void SetColorAttachmentFormats(const VkFormat* formats, size_t count );
	void DisableDepthTest( );
	void EnableDepthTest( bool depthWriteEnable, VkCompareOp op );

	VkPipeline Build( VkDevice device ) const;

	VkPipelineLayout GetLayout() const { return m_pipelineLayout; }
	void SetLayout(const VkPipelineLayout layout) { m_pipelineLayout = layout; }
	
private:
	std::vector<VkPipelineShaderStageCreateInfo> m_shaderStages;

	VkPipelineInputAssemblyStateCreateInfo m_inputAssembly;
	VkPipelineRasterizationStateCreateInfo m_rasterizer;
	VkPipelineColorBlendAttachmentState m_colorBlendAttachment;
	VkPipelineMultisampleStateCreateInfo m_multisampling;
	VkPipelineLayout m_pipelineLayout;
	VkPipelineDepthStencilStateCreateInfo m_depthStencil;
	VkPipelineRenderingCreateInfo m_renderInfo;
	size_t m_attachmentCount = 0;
	VkFormat m_colorAttachmentFormat;

};
