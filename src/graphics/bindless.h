#pragma once

#include <vk_types.h>

class GfxDevice;

class BindlessRegistry {
public:
	void init( GfxDevice& gfx );
	void cleanup( GfxDevice& gfx );

	void addImage( GfxDevice& gfx, ImageID id, const VkImageView view);
	void addSampler( GfxDevice& gfx, uint32_t id, const VkSampler sampler );

	static const size_t MAX_BINDLESS_IMAGES = 100;
	static const size_t MAX_SAMPLERS = 3;
	static const size_t TEXTURE_BINDING = 0;
	static const size_t SAMPLERS_BINDING = 1;

	VkDescriptorPool pool;
	VkDescriptorSetLayout layout;
	VkDescriptorSet set;

	VkSampler nearest_sampler;
	VkSampler linear_sampler;
	VkSampler shadow_map_sampler;

private:
	void initSamplers( GfxDevice& gfx );
};
