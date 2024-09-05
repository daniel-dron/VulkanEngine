#include "ibl.h"

#include <graphics/gfx_device.h>
#include <graphics/image_codex.h>

void IBL::init( GfxDevice& gfx, VkCommandBuffer cmd, const std::string& path ) {
	etc_pipeline.init( gfx, "ibl" );

	loadHdrSkyboxMap( gfx, path );

	irradiance = gfx.image_codex.createCubemap( "IBL SKYBOX", VkExtent3D{ 32, 32, 1 }, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT );
	etc_pipeline.draw( gfx, cmd, getSkyboxImage( ), irradiance);
}

void IBL::clean( GfxDevice& gfx ) {
	etc_pipeline.cleanup( gfx );
}

ImageID IBL::getSkyboxImage( ) const {
	return skybox;
}

ImageID IBL::getIrradianceImage( ) const {
	return irradiance;
}

void IBL::loadHdrSkyboxMap( GfxDevice& gfx, const std::string& path ) {
	skybox = gfx.image_codex.loadHDRFromFile( path, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT, false );
}
