#pragma once

#include <graphics/gpu_image.h>
#include <graphics/bindless.h>
#include <vk_types.h>

class GfxDevice;

class ImageCodex {
public:
	inline static const ImageID INVALID_IMAGE_ID = std::numeric_limits<uint32_t>::max( ) - 1;

	void init( GfxDevice* gfx );
	void cleanup( );

	const std::vector<GpuImage>& getImages( );
	const GpuImage& getImage( ImageID );
	ImageID loadImageFromFile( const std::string& path, VkFormat format, VkImageUsageFlags usage, bool mipmapped );
	ImageID loadHDRFromFile( const std::string& path, VkFormat format, VkImageUsageFlags usage, bool mipmapped );
	ImageID loadImageFromData( const std::string& name, void* data, VkExtent3D extent, VkFormat format, VkImageUsageFlags usage, bool mipmapped );
	ImageID loadCubemapFromFile( const std::vector<std::string>& paths, VkFormat format, VkImageUsageFlags usage, bool mipmapped );
	ImageID loadCubemapFromData( const std::vector<std::string>& paths, const std::vector<unsigned char*> datas, VkExtent3D extent, VkFormat format, VkImageUsageFlags usage, bool mipmapped );

	ImageID createEmptyImage( const std::string& name, VkExtent3D extent, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false );
	ImageID createCubemap( const std::string& name, VkExtent3D extent, VkFormat format, VkImageUsageFlags usage );

	VkDescriptorSetLayout getBindlessLayout( ) const;
	VkDescriptorSet getBindlessSet( ) const;

	ImageID getWhiteImageId( ) const { return white; }
	ImageID getBlackImageId( ) const { return black; }
	ImageID getGreyImageId( ) const { return grey; }
	ImageID getChekboardImageId( ) const { return checkboard; }

	BindlessRegistry bindless_registry;

private:
	void initDefaultImages( );

	ImageID white = INVALID_IMAGE_ID;
	ImageID black = INVALID_IMAGE_ID;
	ImageID grey = INVALID_IMAGE_ID;
	ImageID checkboard = INVALID_IMAGE_ID;

	std::vector<GpuImage> images;
	GfxDevice* gfx;
};