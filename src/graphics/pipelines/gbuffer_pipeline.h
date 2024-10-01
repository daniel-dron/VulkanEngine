#pragma once

#include "pipeline.h"

class GBufferPipeline : public Pipeline {
public:
	virtual Result<> init( GfxDevice& gfx ) override;
	virtual void cleanup( GfxDevice& gfx ) override;
	DrawStats draw( GfxDevice& gfx, VkCommandBuffer cmd, const std::vector<MeshDrawCommand>& draw_commands, const GpuSceneData& scene_data ) const;

private:
	struct PushConstants {
		mat4 world_from_local;
		VkDeviceAddress scene_data_address;
		VkDeviceAddress vertex_buffer_address;
		uint32_t material_id;
	};

	void Reconstruct( GfxDevice& gfx );

	GpuBuffer gpu_scene_data;
};
