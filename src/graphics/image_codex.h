#pragma once

#include <graphics/gpu_image.h>
#include <vk_types.h>

class GfxDevice;

class ImageCodex {
public:
	void init( GfxDevice* gfx);
	void cleanup( );

	const std::vector<GpuImage>& getImages( );
	const GpuImage& getImage( ImageID );
	ImageID loadImageFromFile( const std::string& path, VkFormat format, VkImageUsageFlags usage, bool mipmapped );
	ImageID loadImageFromData( const std::string& name, void* data, VkExtent3D extent, VkFormat format, VkImageUsageFlags usage, bool mipmapped );

private:
	std::vector<GpuImage> images;
	GfxDevice* gfx;
};