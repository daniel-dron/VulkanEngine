#include "image_codex.h"

#include <stb_image.h>
#include <vk_engine.h>
#include <vk_initializers.h>
#include <vk_images.h>

void ImageCodex::init( VulkanEngine* engine ) { this->engine = engine; }

void ImageCodex::cleanup( ) {
	for ( auto& img : images ) {
		vkDestroyImageView( engine->gfx->device, img.view, nullptr );
		vmaDestroyImage( engine->gfx->allocator, img.image, img.allocation );
	}
}

const std::vector<GpuImage>& ImageCodex::getImages( ) { return images; }

const GpuImage& ImageCodex::getImage( ImageID id ) { return images[id]; }

ImageID ImageCodex::loadImageFromFile( const std::string& path, VkFormat format,
	VkImageUsageFlags usage, bool mipmapped ) {
	// search for already loaded image
	for ( const auto img : images ) {
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
	size_t data_size =
		image.extent.depth * image.extent.width * image.extent.height * 4;

	AllocatedBuffer staging_buffer = engine->gfx->allocate(
		data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, __FUNCTION__ );

	void* mapped_buffer = nullptr;
	vmaMapMemory( engine->gfx->allocator, staging_buffer.allocation, &mapped_buffer );
	memcpy( mapped_buffer, data, data_size );
	vmaUnmapMemory( engine->gfx->allocator, staging_buffer.allocation );

	// ----------
	// allocate image
#pragma region allocation
	auto create_info =
		vkinit::image_create_info( format, usage +
			VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, image.extent );
	if ( mipmapped ) {
		create_info.mipLevels = static_cast<uint32_t>(std::floor( std::log2( std::max( image.extent.width, image.extent.height ) ) )) + 1;
	}

	VmaAllocationCreateInfo alloc_info = {
		.usage = VMA_MEMORY_USAGE_GPU_ONLY,
		.requiredFlags = VkMemoryPropertyFlags( VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT )
	};
	VK_CHECK( vmaCreateImage( engine->gfx->allocator, &create_info, &alloc_info, &image.image, &image.allocation, nullptr ) );

	// if the format is for depth, use the correct aspect
	VkImageAspectFlags aspect = format == VK_FORMAT_D32_SFLOAT ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

	auto view_info = vkinit::imageview_create_info( format, image.image, aspect );
	view_info.subresourceRange.levelCount = create_info.mipLevels;
	VK_CHECK( vkCreateImageView( engine->gfx->device, &view_info, nullptr, &image.view ) );
#pragma endregion allocation

	// ----------
	// copy image contents to the allocation
#pragma region copy
	engine->gfx->execute( [&]( VkCommandBuffer cmd ) {
		vkutil::transition_image( cmd, image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL );

		VkBufferImageCopy copy_region = {
			.bufferOffset = 0,
			.bufferRowLength = 0,
			.bufferImageHeight = 0,
			.imageSubresource = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
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
			vkutil::transition_image( cmd, image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
		}
	} );
#pragma endregion copy

	engine->gfx->free( staging_buffer );

	ImageID image_id = images.size( );
	image.id = image_id;
	images.push_back( std::move( image ) );
	return image_id;
}
