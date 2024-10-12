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

#include <vk_types.h>

class GfxDevice;

class BindlessRegistry {
public:
	void Init( GfxDevice& gfx );
	void Cleanup( const GfxDevice & gfx );

	void AddImage( const GfxDevice & gfx, ImageId id, const VkImageView view);
	void AddSampler( GfxDevice& gfx, uint32_t id, const VkSampler sampler );

	static constexpr size_t MaxBindlessImages = 16000;
	static constexpr size_t MaxSamplers = 3;
	static constexpr size_t TextureBinding = 0;
	static constexpr size_t SamplersBinding = 1;

	VkDescriptorPool pool;
	VkDescriptorSetLayout layout;
	VkDescriptorSet set;

	VkSampler nearestSampler;
	VkSampler linearSampler;
	VkSampler shadowMapSampler;

private:
	void InitSamplers( GfxDevice& gfx );
};
