#pragma once

#include <vk_types.h>

class GfxDevice;

struct GpuMaterial {

};

struct Material {
	
};

class MaterialCodex {
public:
	void init( GfxDevice& gfx );
	void cleanup( GfxDevice& gfx );

	MaterialID addMaterial( GfxDevice& gfx, const Material& material );
	const Material& getMaterial( MaterialID id );
private:
	GpuBuffer material_gpu;
	static const size_t MAX_MATERIALS = 1000;

	std::vector<Material> materials;
};