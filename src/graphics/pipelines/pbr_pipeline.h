#pragma once

#include "pipeline.h"
#include <graphics/gbuffer.h>

class PbrPipeline : public Pipeline {
public:
	virtual Result<> init( GfxDevice& gfx ) override;
	virtual void cleanup( GfxDevice& gfx ) override;
	DrawStats draw( GfxDevice& gfx, VkCommandBuffer cmd, const GpuSceneData& scene_data, const GBuffer& gbuffer, uint32_t irradiance_map, uint32_t radiance_map, uint32_t brdf_lut ) const;

private:
	void Reconstruct( GfxDevice& gfx );

	struct PushConstants {
		VkDeviceAddress scene_data_address;
		uint32_t albedo_tex;
		uint32_t normal_tex;
		uint32_t position_tex;
		uint32_t pbr_tex;
		uint32_t irradiance_tex;
		uint32_t radiance_tex;
		uint32_t brdf_lut;
	};

	GpuBuffer gpu_scene_data;
};