#pragma once

#include <vk_types.h>
#include <graphics/image_codex.h>
#include "pipelines/ibl_pipeline.h"

class GfxDevice;

class IBL {
public:
	void init( GfxDevice& gfx, VkCommandBuffer cmd, const std::string& path );
	void clean( GfxDevice& gfx );

	ImageID getSkyboxImage( ) const;
	ImageID getIrradianceImage( ) const;
private:

	void loadHdrSkyboxMap( GfxDevice& gfx, const std::string& path);
	IblPipeline ibl_pipeline;

	ImageID skybox = ImageCodex::INVALID_IMAGE_ID;
	ImageID irradiance = ImageCodex::INVALID_IMAGE_ID;
};