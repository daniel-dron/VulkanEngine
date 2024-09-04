#pragma once

#include "pipeline.h"

class IblPipeline : public Pipeline {
public:
	virtual Result<> init( GfxDevice& gfx ) override;
	virtual void cleanup( GfxDevice& gfx ) override;
	DrawStats draw( GfxDevice& gfx, VkCommandBuffer cmd, ImageID equirectangular, ImageID dst_cubemap ) const;

private:

	void initCubeMesh( GfxDevice& gfx );
	void initPushConstants( GfxDevice& gfx );

	struct PushConstants {
		VkDeviceAddress vertex_buffer_address;
		VkDeviceAddress matrices;
	};

	struct Matrices {
		mat4 projection;
		mat4 views[6];
	};

	PushConstants push_constants;
	GpuBuffer gpu_matrices;
	MeshID cube_mesh;
};
