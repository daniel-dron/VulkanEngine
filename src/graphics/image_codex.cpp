#include "image_codex.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include "gfx_device.h"
#include <vk_initializers.h>
#include <vk_images.h>

void ImageCodex::init( GfxDevice* gfx ) {
	this->gfx = gfx;
	bindless_registry.init( *this->gfx );

	initDefaultImages( );
}

void ImageCodex::cleanup( ) {
	for ( auto& img : images ) {
		vkDestroyImageView( gfx->device, img.view, nullptr );

		for ( auto view : img.mip_views ) {
			if ( view ) {
				vkDestroyImageView( gfx->device, view, nullptr );
			}
		}

		vmaDestroyImage( gfx->allocator, img.image, img.allocation );
	}

	bindless_registry.cleanup( *gfx );
}

const std::vector<GpuImage>& ImageCodex::getImages( ) { return images; }

const GpuImage& ImageCodex::getImage( ImageID id ) { return images[id]; }

ImageID ImageCodex::loadImageFromFile( const std::string& path, VkFormat format, VkImageUsageFlags usage, bool mipmapped ) {
	// search for already loaded image
	for ( const auto& img : images ) {
		if ( path == img.info.path && format == img.format && usage == img.usage &&
			mipmapped == img.mipmapped ) {
			return img.id;
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
		if ( path == img.info.path && format == img.format && usage == img.usage &&
			mipmapped == img.mipmapped ) {
			return img.id;
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
		if ( paths[0] == img.info.path && format == img.format && usage == img.usage && mipmapped == img.mipmapped ) {
			return img.id;
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
	GpuImage image;

	image.cubemap = true;
	image.extent = extent;
	image.usage = usage;
	image.mipmapped = mipmapped;
	image.info.debug_name = paths.at( 0 );

	// ----------
	// staging
	size_t image_size = image.extent.width * image.extent.height * 4;
	if ( format == VK_FORMAT_R16G16B16A16_SFLOAT ) {
		image_size = image_size * 2;
	}

	size_t data_size = 6 * image_size;

	auto staging_buffer = gfx->allocate( data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, __FUNCTION__ );

	void* mapped_buffer = nullptr;
	vmaMapMemory( gfx->allocator, staging_buffer.allocation, &mapped_buffer );
	// copy each image to its respective place in the staging buffer
	for ( size_t i = 0; i < datas.size( ); i++ ) {
		auto data = datas.at( i );
		void* dst_buffer = (void*)(((uintptr_t)mapped_buffer) + i * image_size);
		memcpy( dst_buffer, data, image_size );
	}
	vmaUnmapMemory( gfx->allocator, staging_buffer.allocation );

	// ---------
	// Allocate image
	VkImageCreateInfo create_info = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.pNext = nullptr,
		.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = format,
		.extent = extent,
		.mipLevels = 1,
		.arrayLayers = 6,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
	};
	if ( mipmapped ) {
		create_info.mipLevels = static_cast<uint32_t>(std::floor( std::log2( std::max( image.extent.width, image.extent.height ) ) )) + 1;
	}

	VmaAllocationCreateInfo alloc_info = {
		.usage = VMA_MEMORY_USAGE_GPU_ONLY,
		.requiredFlags = VkMemoryPropertyFlags( VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT )
	};
	VK_CHECK( vmaCreateImage( gfx->allocator, &create_info, &alloc_info, &image.image, &image.allocation, nullptr ) );
#ifdef ENABLE_DEBUG_UTILS
	const VkDebugUtilsObjectNameInfoEXT obj = {
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
		.pNext = nullptr,
		.objectType = VkObjectType::VK_OBJECT_TYPE_IMAGE,
		.objectHandle = (uint64_t)image.image,
		.pObjectName = paths.at( 0 ).c_str( )
	};
	vkSetDebugUtilsObjectNameEXT( gfx->device, &obj );
#endif

	VkImageViewCreateInfo view_info = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.pNext = nullptr,
		.image = image.image,
		.viewType = VK_IMAGE_VIEW_TYPE_CUBE,
		.format = format,
		.subresourceRange = VkImageSubresourceRange {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = create_info.mipLevels,
			.baseArrayLayer = 0,
			.layerCount = 6
		},
	};
	VK_CHECK( vkCreateImageView( gfx->device, &view_info, nullptr, &image.view ) );

	//----------
	//Copy from staging buffers
	gfx->execute( [&]( VkCommandBuffer cmd ) {
		vkutil::transition_image( cmd, image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL );

		std::vector<VkBufferImageCopy> copy_regions;

		for ( uint32_t face = 0; face < 6; face++ ) {
			size_t offset = face * image_size;

			VkBufferImageCopy copy_region = {
				.bufferOffset = offset,
				.imageSubresource = VkImageSubresourceLayers {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.mipLevel = 0,
					.baseArrayLayer = face,
					.layerCount = 1,
				},
				.imageExtent = VkExtent3D {
					.width = extent.width,
					.height = extent.height,
					.depth = 1
				},
			};

			copy_regions.push_back( copy_region );
		}

		vkCmdCopyBufferToImage( cmd, staging_buffer.buffer, image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, copy_regions.size( ), copy_regions.data( ) );

		vkutil::transition_image( cmd, image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL );
	} );

	gfx->free( staging_buffer );

	ImageID image_id = images.size( );
	image.id = image_id;

	bindless_registry.addImage( *gfx, image_id, image.view );
	images.push_back( std::move( image ) );

	return image_id;
}

ImageID ImageCodex::createCubemap( const std::string& name, VkExtent3D extent, VkFormat format, VkImageUsageFlags usage, int mipmaps ) {
	GpuImage image;
	image.cubemap = true;
	image.extent = extent;
	image.usage = usage;
	image.mipmapped = mipmaps != 0;
	image.info.debug_name = name;

	// Calculate the number of mip levels if not specified
	if ( mipmaps == 0 ) {
		mipmaps = static_cast<int>(std::floor( std::log2( std::max( extent.width, extent.height ) ) )) + 1;
	}

	// Allocate image
	VkImageCreateInfo create_info = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.pNext = nullptr,
		.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = format,
		.extent = extent,
		.mipLevels = static_cast<uint32_t>(mipmaps),
		.arrayLayers = 6,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
	};

	VmaAllocationCreateInfo alloc_info = {
		.usage = VMA_MEMORY_USAGE_GPU_ONLY,
		.requiredFlags = VkMemoryPropertyFlags( VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT )
	};
	VK_CHECK( vmaCreateImage( gfx->allocator, &create_info, &alloc_info, &image.image, &image.allocation, nullptr ) );

#ifdef ENABLE_DEBUG_UTILS
	const VkDebugUtilsObjectNameInfoEXT obj = {
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
		.pNext = nullptr,
		.objectType = VkObjectType::VK_OBJECT_TYPE_IMAGE,
		.objectHandle = (uint64_t)image.image,
		.pObjectName = name.c_str( )
	};
	vkSetDebugUtilsObjectNameEXT( gfx->device, &obj );
#endif

	// Create the main cubemap view
	VkImageViewCreateInfo view_info = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.pNext = nullptr,
		.image = image.image,
		.viewType = VK_IMAGE_VIEW_TYPE_CUBE,
		.format = format,
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = static_cast<uint32_t>(mipmaps),
			.baseArrayLayer = 0,
			.layerCount = 6
		},
	};
	VK_CHECK( vkCreateImageView( gfx->device, &view_info, nullptr, &image.view ) );

	// Create views for individual mip levels
	for ( int i = 0; i < mipmaps; i++ ) {
		VkImageView view;
		VkImageViewCreateInfo mip_view_info = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.pNext = nullptr,
			.image = image.image,
			.viewType = VK_IMAGE_VIEW_TYPE_CUBE,
			.format = format,
			.subresourceRange = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = static_cast<uint32_t>(i),
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 6
			},
		};
		VK_CHECK( vkCreateImageView( gfx->device, &mip_view_info, nullptr, &view ) );
		image.mip_views.push_back( view );
	}

	gfx->execute( [&]( VkCommandBuffer cmd ) {
		vkutil::transition_image( cmd, image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, false );
	} );

	ImageID image_id = images.size( );
	image.id = image_id;
	bindless_registry.addImage( *gfx, image_id, image.view );
	images.push_back( std::move( image ) );
	return image_id;
}

ImageID ImageCodex::createEmptyImage( const std::string& name, VkExtent3D extent, VkFormat format, VkImageUsageFlags usage, bool mipmapped ) {
	GpuImage image;
	// ----------
	// data
	image.extent = extent;
	image.format = format;
	image.usage = usage;
	image.mipmapped = mipmapped;
	image.info.debug_name = name;
	// ----------
	// allocate image
	auto create_info = vkinit::image_create_info( format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, image.extent );
	if ( mipmapped ) {
		create_info.mipLevels = static_cast<uint32_t>(std::floor( std::log2( std::max( image.extent.width, image.extent.height ) ) )) + 1;
	}

	VmaAllocationCreateInfo alloc_info = {
		.usage = VMA_MEMORY_USAGE_GPU_ONLY,
		.requiredFlags = VkMemoryPropertyFlags( VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT )
	};
	VK_CHECK( vmaCreateImage( gfx->allocator, &create_info, &alloc_info, &image.image, &image.allocation, nullptr ) );
#ifdef ENABLE_DEBUG_UTILS
	const VkDebugUtilsObjectNameInfoEXT obj = {
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
		.pNext = nullptr,
		.objectType = VkObjectType::VK_OBJECT_TYPE_IMAGE,
		.objectHandle = (uint64_t)image.image,
		.pObjectName = name.c_str( )
	};
	vkSetDebugUtilsObjectNameEXT( gfx->device, &obj );
#endif
	// if the format is for depth, use the correct aspect
	VkImageAspectFlags aspect = format == VK_FORMAT_D32_SFLOAT ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
	auto depth = aspect & VK_IMAGE_ASPECT_DEPTH_BIT;
	auto view_info = vkinit::imageview_create_info( format, image.image, aspect );
	view_info.subresourceRange.levelCount = create_info.mipLevels;
	VK_CHECK( vkCreateImageView( gfx->device, &view_info, nullptr, &image.view ) );

	// ----------
	// Initialize image layout
	gfx->execute( [&]( VkCommandBuffer cmd ) {
		VkImageLayout finalLayout = depth ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		vkutil::transition_image( cmd, image.image, VK_IMAGE_LAYOUT_UNDEFINED, finalLayout, depth );
	} );

	ImageID image_id = images.size( );
	image.id = image_id;
	bindless_registry.addImage( *gfx, image_id, image.view );
	images.push_back( std::move( image ) );
	return image_id;
}

ImageID ImageCodex::loadImageFromData( const std::string& name, void* data, VkExtent3D extent, VkFormat format, VkImageUsageFlags usage, bool mipmapped ) {
	GpuImage image;

	// ----------
	// data
	image.extent = extent;
	image.format = format;
	image.usage = usage;
	image.mipmapped = mipmapped;
	image.info.debug_name = name;

	// ----------
	// allocate
	size_t data_size = image.extent.depth * image.extent.width * image.extent.height * 4;
	if ( format == VK_FORMAT_R16G16B16A16_SFLOAT ) {
		data_size = data_size * 2;
	} else if ( format == VK_FORMAT_R32G32B32A32_SFLOAT ) {
		data_size = data_size * 4;
	}

	GpuBuffer staging_buffer = gfx->allocate(
		data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, __FUNCTION__ );

	void* mapped_buffer = nullptr;
	vmaMapMemory( gfx->allocator, staging_buffer.allocation, &mapped_buffer );
	memcpy( mapped_buffer, data, data_size );
	vmaUnmapMemory( gfx->allocator, staging_buffer.allocation );

	// ----------
	// allocate image
#pragma region allocation
	auto create_info =
		vkinit::image_create_info( format, usage |
			VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, image.extent );
	if ( mipmapped ) {
		create_info.mipLevels = static_cast<uint32_t>(std::floor( std::log2( std::max( image.extent.width, image.extent.height ) ) )) + 1;
	}

	VmaAllocationCreateInfo alloc_info = {
		.usage = VMA_MEMORY_USAGE_GPU_ONLY,
		.requiredFlags = VkMemoryPropertyFlags( VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT )
	};
	VK_CHECK( vmaCreateImage( gfx->allocator, &create_info, &alloc_info, &image.image, &image.allocation, nullptr ) );
#ifdef ENABLE_DEBUG_UTILS
	const VkDebugUtilsObjectNameInfoEXT obj = {
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
		.pNext = nullptr,
		.objectType = VkObjectType::VK_OBJECT_TYPE_IMAGE,
		.objectHandle = (uint64_t)image.image,
		.pObjectName = name.c_str( )
	};
	vkSetDebugUtilsObjectNameEXT( gfx->device, &obj );
#endif

	// if the format is for depth, use the correct aspect
	VkImageAspectFlags aspect = format == VK_FORMAT_D32_SFLOAT ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
	auto depth = aspect & VK_IMAGE_ASPECT_DEPTH_BIT;

	auto view_info = vkinit::imageview_create_info( format, image.image, aspect );
	view_info.subresourceRange.levelCount = create_info.mipLevels;
	VK_CHECK( vkCreateImageView( gfx->device, &view_info, nullptr, &image.view ) );
#pragma endregion allocation

	// ----------
	// copy image contents to the allocation
#pragma region copy
	gfx->execute( [&]( VkCommandBuffer cmd ) {
		vkutil::transition_image( cmd, image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, depth );

		VkBufferImageCopy copy_region = {
			.bufferOffset = 0,
			.bufferRowLength = 0,
			.bufferImageHeight = 0,
			.imageSubresource = {
				.aspectMask = aspect,
				.mipLevel = 0,
				.baseArrayLayer = 0,
				.layerCount = 1
			},
			.imageExtent = image.extent
		};

		vkCmdCopyBufferToImage( cmd, staging_buffer.buffer, image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region );

		if ( mipmapped ) {
			vkutil::generate_mipmaps( cmd, image.image, VkExtent2D{ image.extent.width, image.extent.height } );
		} else {
			vkutil::transition_image( cmd, image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, depth );
		}
	} );
#pragma endregion copy

	gfx->free( staging_buffer );

	ImageID image_id = images.size( );
	image.id = image_id;

	bindless_registry.addImage( *gfx, image_id, image.view );
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
