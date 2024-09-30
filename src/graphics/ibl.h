#pragma once

#include <vk_types.h>
#include <graphics/image_codex.h>
#include <graphics/pipelines/compute_pipeline.h>

class GfxDevice;

class IBL {
public:
	void init( GfxDevice& gfx, const std::string& path );
	void clean( GfxDevice& gfx );

	ImageID getSkybox( ) const { return skybox; }
	ImageID getIrradiance( ) const { return irradiance; }
	ImageID getRadiance( ) const { return radiance; }
	ImageID getBRDF( ) const { return brdf; }
private:
	void initComputes( GfxDevice& gfx );
	void initTextures( GfxDevice& gfx );

	void generateSkybox( GfxDevice& gfx, VkCommandBuffer cmd ) const;
	void generateIrradiance( GfxDevice& gfx, VkCommandBuffer cmd ) const;
	void generateRadiance( GfxDevice& gfx, VkCommandBuffer cmd ) const;
	void generateBrdf( GfxDevice& gfx, VkCommandBuffer cmd ) const;

	VkCommandBuffer compute_command;
	VkFence compute_fence;

	ImageID hdr_texture = ImageCodex::INVALID_IMAGE_ID; // 2D

	ImageID skybox = ImageCodex::INVALID_IMAGE_ID;		// Cubemap
	ImageID irradiance = ImageCodex::INVALID_IMAGE_ID;	// Cubemap
	ImageID radiance = ImageCodex::INVALID_IMAGE_ID;	// Cubemap
	ImageID brdf = ImageCodex::INVALID_IMAGE_ID;		// 2D

	BindlessCompute irradiance_pipeline;
	VkDescriptorSet irradiance_set;

	BindlessCompute equirectangular_pipeline;
	VkDescriptorSet equi_set;

	struct RadiancePushConstants {
		ImageID input;
		int mipmap;
		float roughness;
	};
	BindlessCompute radiance_pipeline;
	std::array<VkDescriptorSet, 6> radiance_sets;

	BindlessCompute brdf_pipeline;
	VkDescriptorSet brdf_set;

};