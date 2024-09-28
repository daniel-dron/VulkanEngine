#include "image_codex.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include "gfx_device.h"
#include <vk_initializers.h>

void ImageCodex::init( GfxDevice* gfx ) {
	this->gfx = gfx;
	bindless_registry.init( *this->gfx );

	initDefaultImages( );
}

void ImageCodex::cleanup( ) {
	images.clear( );
	bindless_registry.cleanup( *gfx );
}

const std::vector<GpuImage>& ImageCodex::getImages( ) { return images; }

const GpuImage& ImageCodex::getImage( ImageID id ) { return images[id]; }

ImageID ImageCodex::loadImageFromFile( const std::string& path, VkFormat format, VkImageUsageFlags usage, bool mipmapped ) {
	// search for already loaded image
	for ( const auto& img : images ) {
		if ( path == img.GetName( ) && format == img.GetFormat( ) && usage == img.GetUsage( ) &&
			mipmapped == img.IsMipmapped( ) ) {
			return img.GetId( );
		}
	}

	int width, height, nr_channels;
	unsigned char* data =
		stbi_load( path.c_str( ), &width, &height, &nr_channels, 4 );
	if ( !data ) {
		return -1;
	}

	VkExtent3D extent = {
		.width = static_cast<uint32_t>(width),
		.height = static_cast<uint32_t>(height),
		.depth = 1
	};

	auto id = loadImageFromData( path, data, extent, format, usage, mipmapped );

	stbi_image_free( data );

	return id;
}

ImageID ImageCodex::loadHDRFromFile( const std::string& path, VkFormat format, VkImageUsageFlags usage, bool mipmapped ) {
	// search for already loaded image
	for ( const auto& img : images ) {
		if ( path == img.GetName( ) && format == img.GetFormat( ) && usage == img.GetUsage( ) &&
			mipmapped == img.IsMipmapped( ) ) {
			return img.GetId( );
		}
	}

	int width, height, nr_channels;
	float* data = stbi_loadf( path.c_str( ), &width, &height, &nr_channels, 4 );
	if ( !data ) {
		return -1;
	}

	VkExtent3D extent = {
		.width = static_cast<uint32_t>(width),
		.height = static_cast<uint32_t>(height),
		.depth = 1
	};

	auto id = loadImageFromData( path, data, extent, format, usage, mipmapped );

	stbi_image_free( data );

	return id;
}

ImageID ImageCodex::loadCubemapFromFile( const std::vector<std::string>& paths, VkFormat format, VkImageUsageFlags usage, bool mipmapped ) {
	assert( paths.size( ) == 6 && "Cubemap needs 6 faces!" );

	for ( const auto& img : images ) {
		if ( paths[0] == img.GetName( ) && format == img.GetFormat( ) && usage == img.GetUsage( ) && mipmapped == img.IsMipmapped( ) ) {
			return img.GetId( );
		}
	}

	VkExtent3D extent = {
		.width = 0,
		.height = 0,
		.depth = 1
	};

	stbi_set_flip_vertically_on_load( true );
	std::vector<unsigned char*> datas;
	for ( size_t i = 0; i < 6; i++ ) {
		auto& path = paths.at( i );

		int width, height, nr_channels;
		unsigned char* data = stbi_load( path.c_str( ), &width, &height, &nr_channels, 4 );
		if ( !data ) {
			return -1;
		}

		extent.width = width;
		extent.height = height;

		datas.push_back( data );
	}

	auto image_id = loadCubemapFromData( paths, datas, extent, format, usage, mipmapped );
	stbi_set_flip_vertically_on_load( false );

	for ( const auto data : datas ) {
		stbi_image_free( data );
	}

	return image_id;
}

ImageID ImageCodex::loadCubemapFromData( const std::vector<std::string>& paths, const std::vector<unsigned char*> datas, VkExtent3D extent, VkFormat format, VkImageUsageFlags usage, bool mipmapped ) {
	size_t total_size = Image::CalculateSize( extent, format ) * datas.size( );
	size_t face_size = Image::CalculateSize( extent, format );

	unsigned char* merged_data = new unsigned char[total_size];
	size_t offset = {};

	for ( const auto& data : datas ) {
		std::memcpy( merged_data + offset, data, face_size );
		offset += face_size;
	}

	GpuImage image = GpuImage( gfx, paths.at( 0 ), (void*)merged_data, extent, format, ImageType::T_CUBEMAP, usage, mipmapped );

	delete[] merged_data;

	ImageID image_id = images.size( );
	image.SetId( image_id );

	bindless_registry.addImage( *gfx, image_id, image.GetBaseView( ) );
	images.push_back( std::move( image ) );

	return image_id;
}

ImageID ImageCodex::createCubemap( const std::string& name, VkExtent3D extent, VkFormat format, VkImageUsageFlags usage, int mipmaps ) {
	GpuImage image = GpuImage( gfx, name, extent, format, ImageType::T_CUBEMAP, usage, mipmaps );

	ImageID image_id = images.size( );
	image.SetId( image_id );

	bindless_registry.addImage( *gfx, image_id, image.GetBaseView( ) );
	images.push_back( std::move( image ) );
	return image_id;
}

ImageID ImageCodex::createEmptyImage( const std::string& name, VkExtent3D extent, VkFormat format, VkImageUsageFlags usage, bool mipmapped ) {
	GpuImage image = GpuImage( gfx, name, extent, format, ImageType::T_2D, usage, mipmapped );

	ImageID image_id = images.size( );
	image.SetId( image_id );

	bindless_registry.addImage( *gfx, image_id, image.GetBaseView( ) );
	images.push_back( std::move( image ) );
	return image_id;
}

ImageID ImageCodex::loadImageFromData( const std::string& name, void* data, VkExtent3D extent, VkFormat format, VkImageUsageFlags usage, bool mipmapped ) {
	GpuImage image = GpuImage( gfx, name, data, extent, format, ImageType::T_2D, usage, mipmapped );

	ImageID image_id = images.size( );
	image.SetId( image_id );

	bindless_registry.addImage( *gfx, image_id, image.GetBaseView( ) );
	images.push_back( std::move( image ) );

	return image_id;
}

VkDescriptorSetLayout ImageCodex::getBindlessLayout( ) const {
	return bindless_registry.layout;
}

VkDescriptorSet ImageCodex::getBindlessSet( ) const {
	return bindless_registry.set;
}

void ImageCodex::initDefaultImages( ) {
	// 3 default textures, white, grey, black. 1 pixel each
	uint32_t white = glm::packUnorm4x8( glm::vec4( 1, 1, 1, 1 ) );
	white = loadImageFromData( "debug_white_img", (void*)&white, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, false );

	uint32_t grey = glm::packUnorm4x8( glm::vec4( 0.66f, 0.66f, 0.66f, 1 ) );
	grey = loadImageFromData( "debug_grey_img", (void*)&grey, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, false );

	uint32_t black = glm::packUnorm4x8( glm::vec4( 0, 0, 0, 0 ) );
	black = loadImageFromData( "debug_black_img", (void*)&white, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, false );

	// checkerboard image
	uint32_t magenta = glm::packUnorm4x8( glm::vec4( 1, 0, 1, 1 ) );
	std::array<uint32_t, 16 * 16> pixels;  // for 16x16 checkerboard texture
	for ( int x = 0; x < 16; x++ ) {
		for ( int y = 0; y < 16; y++ ) {
			pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
		}
	}
	checkboard = loadImageFromData( "debug_checkboard_img", (void*)&white, VkExtent3D{ 16, 16, 1 }, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, false );
}
