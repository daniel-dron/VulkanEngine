#pragma once

#include "pipeline.h"

class SkyboxPipeline : public Pipeline {
public:
	virtual Result<> init( GfxDevice& gfx ) override;
	virtual void cleanup( GfxDevice& gfx ) override;

	void draw( GfxDevice& gfx, VkCommandBuffer cmd, ImageID skybox_texture, const GpuSceneData& scene_data ) const;

private:
	struct PushConstants {
		VkDeviceAddress scene_data_address;
		VkDeviceAddress vertex_buffer_address;
		uint32_t texture_id;
	};

	void createCubeMesh( GfxDevice& gfx );

	MeshID cube_mesh;
	GpuBuffer gpu_scene_data;
};