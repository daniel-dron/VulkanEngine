#pragma once

#include <vk_types.h>
#include <graphics/image_codex.h>
#include <graphics/pipelines/ibl_pipeline.h>
#include <graphics/pipelines/compute_pipeline.h>

class GfxDevice;

class IBL {
public:
	void init( GfxDevice& gfx, VkCommandBuffer cmd, const std::string& path );
	void clean( GfxDevice& gfx );

	ImageID getSkybox( ) const { return skybox; }
private:
	void initComputes( GfxDevice& gfx );
	void initTextures( GfxDevice& gfx );

	void generateSkybox( GfxDevice& gfx, VkCommandBuffer cmd ) const;

	ImageID hdr_texture = ImageCodex::INVALID_IMAGE_ID; // 2D

	ImageID skybox = ImageCodex::INVALID_IMAGE_ID;		// Cubemap
	ImageID irradiance = ImageCodex::INVALID_IMAGE_ID;	// Cubemap
	ImageID brdf = ImageCodex::INVALID_IMAGE_ID;		// 2D

	BindlessComputePipeline<ImageID> equi_pipeline;

};