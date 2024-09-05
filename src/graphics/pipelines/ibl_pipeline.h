#pragma once

#include "pipeline.h"

class EquiToCubePipeline {
public:
	void init( GfxDevice& gfx, const std::string& shader );
	void cleanup( GfxDevice& gfx );

	void draw( GfxDevice& gfx, VkCommandBuffer cmd, ImageID equirectangular, ImageID dst_cubemap );
private:
	VkPipeline pipeline;
	VkPipelineLayout layout;

	struct PushConstants {
		VkDeviceAddress vertex_buffer_address;
		VkDeviceAddress matrices;
		uint32_t skybox;
	};

	struct Matrices {
		mat4 projection;
		mat4 views[6];
	};

	PushConstants push_constants;
	GpuBuffer gpu_matrices;
	MeshID cube_mesh;
};
