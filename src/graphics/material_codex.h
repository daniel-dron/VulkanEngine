#pragma once

#include <vk_types.h>

class GfxDevice;

struct GpuMaterial {
	vec4 base_color;
	vec4 metal_roughness_factors;
	uint32_t color_tex;
	uint32_t metal_roughness_tex;
	uint32_t normal_tex;
	uint32_t pad;
};

struct Material {
	vec4 base_color;
	float metalness_factor;
	float roughness_factor;

	ImageID color_id;
	ImageID metal_roughness_id;
	ImageID normal_id;
};

class MaterialCodex {
public:
	void init( GfxDevice& gfx );
	void cleanup( GfxDevice& gfx );

	MaterialID addMaterial( GfxDevice& gfx, const Material& material );
	const Material& getMaterial( MaterialID id );

	const GpuBuffer& getGpuBuffer( ) const { return material_gpu; }
	VkDeviceAddress getDeviceAddress( ) const { return gpu_address; }

private:
	GpuBuffer material_gpu;
	static const size_t MAX_MATERIALS = 1000;
	VkDeviceAddress gpu_address;

	std::vector<Material> materials;
};