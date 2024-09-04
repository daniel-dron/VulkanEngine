#pragma once

#include "pipeline.h"
#include <graphics/gbuffer.h>

class PbrPipeline : public Pipeline {
public:
	virtual Result<> init( GfxDevice& gfx ) override;
	virtual void cleanup( GfxDevice& gfx ) override;
	DrawStats draw( GfxDevice& gfx, VkCommandBuffer cmd, const GpuSceneData& scene_data, const GBuffer& gbuffer ) const;

private:
	struct PushConstants {
		VkDeviceAddress scene_data_address;
		uint32_t albedo_tex;
		uint32_t normal_tex;
		uint32_t position_tex;
		uint32_t pbr_tex;
	};

	GpuBuffer gpu_scene_data;
};