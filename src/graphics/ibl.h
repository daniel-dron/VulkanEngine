#pragma once

#include <vk_types.h>
#include <graphics/image_codex.h>

class GfxDevice;

class IBL {
public:
	void init( GfxDevice& gfx, const std::string& path );

	ImageID getSkyboxImage( ) const;
private:

	void loadHdrSkyboxMap( GfxDevice& gfx, const std::string& path);

	ImageID skybox = ImageCodex::INVALID_IMAGE_ID;
};