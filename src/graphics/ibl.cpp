#include "ibl.h"

#include <graphics/gfx_device.h>
#include <graphics/image_codex.h>

void IBL::init( GfxDevice& gfx, const std::string& path ) {
	
	loadHdrSkyboxMap( gfx, path );

}

ImageID IBL::getSkyboxImage( ) const {
	return skybox;
}

void IBL::loadHdrSkyboxMap( GfxDevice& gfx, const std::string& path ) {
	skybox = gfx.image_codex.loadHDRFromFile( path, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT, false );
}
