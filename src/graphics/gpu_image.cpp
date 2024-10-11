#include <pch.h>

#include "gpu_image.h"

#include <vk_types.h>
#include <graphics/gfx_device.h>

GpuImage::GpuImage( GfxDevice* gfx, const std::string& name, void* data, VkExtent3D extent, VkFormat format, ImageType image_type, VkImageUsageFlags usage, bool generate_mipmaps )
	: gfx(gfx), name( name ), extent( extent ), format( format ), usage( usage ), mipmapped( generate_mipmaps ) {

	assert( data != nullptr );
	assert( extent.width > 0 && extent.height > 0 );
	//assert( !name.empty( ) );

	this->usage = usage | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

	if ( image_type == ImageType::T_2D ) {
		_ActuallyCreateImage2DFromData(  data );
	} else if ( image_type == ImageType::T_CUBEMAP ) {
		_ActuallyCreateCubemapFromData( data );
	}
}

GpuImage::GpuImage( GfxDevice* gfx, const std::string& name, VkExtent3D extent, VkFormat format, ImageType image_type, VkImageUsageFlags usage, bool generate_mipmaps )
	: gfx(gfx), name( name ), extent( extent ), format( format ), usage( usage ), mipmapped( generate_mipmaps ) {

	assert( extent.width > 0 && extent.height > 0 );
	assert( !name.empty( ) );

	this->usage = usage | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

	if ( image_type == ImageType::T_2D ) {
		_ActuallyCreateEmptyImage2D( );
	} else if ( image_type == ImageType::T_CUBEMAP ) {
		_ActuallyCreateEmptyCubemap( );
	}
}

GpuImage::~GpuImage( ) {
	if ( !gfx ) {
		return;
	}

	vkDestroyImageView( gfx->device, GetBaseView( ), nullptr );

	for ( const auto view : GetMipViews( ) ) {
		vkDestroyImageView( gfx->device, view, nullptr );
	}

	vmaDestroyImage( gfx->allocator, GetImage( ), GetAllocation( ) );
}

void GpuImage::TransitionLayout( VkCommandBuffer cmd, VkImageLayout current_layout, VkImageLayout new_layout, bool depth ) const {
	Image::TransitionLayout( cmd, image, current_layout, new_layout, depth );
}

void GpuImage::GenerateMipmaps( VkCommandBuffer cmd ) const {
	Image::GenerateMipmaps( cmd, image, VkExtent2D{ extent.width, extent.height } );
}

void GpuImage::_SetDebugName( const std::string& name ) {
	const VkDebugUtilsObjectNameInfoEXT obj = {
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
		.pNext = nullptr,
		.objectType = VkObjectType::VK_OBJECT_TYPE_IMAGE,
		.objectHandle = (uint64_t)image,
		.pObjectName = name.c_str( )
	};
	vkSetDebugUtilsObjectNameEXT( gfx->device, &obj );
}

void GpuImage::_ActuallyCreateEmptyImage2D() {
	const bool depth = format == VK_FORMAT_D32_SFLOAT;
	VkImageAspectFlags aspect = depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

	uint32_t mip_levels = 1;
	if ( mipmapped ) {
		mip_levels = static_cast<uint32_t>(std::floor( std::log2( std::max( extent.width, extent.height ) ) )) + 1;
	}

	// ----------
	// allocate vulkan image
	Image::Allocate2D( gfx->allocator, format, extent, this->usage, mip_levels, &image, &allocation );

	_SetDebugName( name );

	gfx->execute( [&]( VkCommandBuffer cmd ) {
		VkImageLayout final_layout = depth ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		TransitionLayout( cmd, VK_IMAGE_LAYOUT_UNDEFINED, final_layout, depth );
	} );

	// ----------
	// create view
	view = Image::CreateView2D( gfx->device, image, format, aspect );

	type = T_2D;
}

void GpuImage::_ActuallyCreateImage2DFromData( void* data ) {
	const size_t data_size = Image::CalculateSize( extent, format );
	const bool depth = format == VK_FORMAT_D32_SFLOAT;
	VkImageAspectFlags aspect = depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

	uint32_t mip_levels = 1;
	if ( mipmapped ) {
		mip_levels = static_cast<uint32_t>(std::floor( std::log2( std::max( extent.width, extent.height ) ) )) + 1;
	}

	// ----------
	// allocate vulkan image
	Image::Allocate2D( gfx->allocator, format, extent, this->usage, mip_levels, &image, &allocation );

	_SetDebugName( name );

	// ---------
	// upload buffer to image & generate mipmaps if needed
	GpuBuffer staging_buffer = gfx->allocate( data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, __FUNCTION__ );
	staging_buffer.Upload( *gfx, data, data_size );

	gfx->execute( [&]( VkCommandBuffer cmd ) {
		TransitionLayout( cmd, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, depth );

		Image::CopyFromBuffer( cmd, staging_buffer.buffer, image, extent, aspect );

		if ( mipmapped ) {
			GenerateMipmaps( cmd );
			TransitionLayout( cmd, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL );
		} else {
			TransitionLayout( cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL );
		}
	} );
	gfx->free( staging_buffer );

	// ----------
	// create views
	view = Image::CreateView2D( gfx->device, image, format, aspect );

	type = T_2D;
}

void GpuImage::_ActuallyCreateCubemapFromData( void* data ) {
	const size_t face_size = Image::CalculateSize( extent, format );
	const size_t data_size = face_size * 6;
	const bool depth = format == VK_FORMAT_D32_SFLOAT;

	VkImageAspectFlags aspect = depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

	uint32_t mip_levels = 1;
	if ( mipmapped ) {
		mip_levels = static_cast<uint32_t>(std::floor( std::log2( std::max( extent.width, extent.height ) ) )) + 1;
	}

	Image::AllocateCubemap( gfx->allocator, format, extent, this->usage, mip_levels, &image, &allocation );

	_SetDebugName( name );

	// ----------
	// upload data & generate mipmaps
	GpuBuffer staging_buffer = gfx->allocate( data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, __FUNCTION__ );
	staging_buffer.Upload( *gfx, data, data_size );

	gfx->execute( [&]( VkCommandBuffer cmd ) {
		TransitionLayout( cmd, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, depth );

		for ( uint32_t face = 0; face < 6; face++ ) {
			size_t offset = face * face_size;

			Image::CopyFromBuffer( cmd, staging_buffer.buffer, image, extent, aspect, offset, face );
		}

		if ( mipmapped ) {
			GenerateMipmaps( cmd );
			TransitionLayout( cmd, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL );
		} else {
			TransitionLayout( cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL );
		}
	} );
	gfx->free( staging_buffer );

	// ----------
	// create views
	for ( int i = 0; i < mip_levels; i++ ) {
		mip_views.push_back( Image::CreateViewCubemap( gfx->device, image, format, aspect, i ) );
	}

	view = Image::CreateViewCubemap( gfx->device, image, format, aspect );

	type = T_CUBEMAP;
}

void GpuImage::_ActuallyCreateEmptyCubemap( ) {
	const size_t face_size = Image::CalculateSize( extent, format );
	const size_t data_size = face_size * 6;
	const bool depth = format == VK_FORMAT_D32_SFLOAT;

	VkImageAspectFlags aspect = depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

	uint32_t mip_levels = 1;
	if ( mipmapped ) {
		mip_levels = static_cast<uint32_t>(std::floor( std::log2( std::max( extent.width, extent.height ) ) )) + 1;
	}

	Image::AllocateCubemap( gfx->allocator, format, extent, this->usage, mip_levels, &image, &allocation );

	_SetDebugName( name );

	// ----------
	// upload data & generate mipmaps
	gfx->execute( [&]( VkCommandBuffer cmd ) {
		TransitionLayout( cmd, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, depth );

		if ( mipmapped ) {
			GenerateMipmaps( cmd );
			TransitionLayout( cmd, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL );
		} else {
			TransitionLayout( cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL );
		}
	} );

	// ----------
	// create views
	for ( int i = 0; i < mip_levels; i++ ) {
		mip_views.push_back( Image::CreateViewCubemap( gfx->device, image, format, aspect, i ) );
	}

	view = Image::CreateViewCubemap( gfx->device, image, format, aspect );

	type = T_CUBEMAP;
}

size_t Image::CalculateSize( VkExtent3D extent, VkFormat format ) {
	size_t data_size = static_cast<size_t>(extent.width) * extent.height * extent.depth;

	switch ( format ) {
	case VK_FORMAT_R16G16B16A16_SFLOAT:
		data_size = data_size * 4 * 2;
		break;
	case VK_FORMAT_R32G32B32A32_SFLOAT:
		data_size = data_size * 4 * 4;
		break;
	case VK_FORMAT_R8G8B8A8_UNORM:
	case VK_FORMAT_R8G8B8A8_SRGB:
		data_size = data_size * 4;
		break;
	case VK_FORMAT_D32_SFLOAT:
		data_size = data_size * 4;
		break;
	default:
		assert( false && "Unknown format for size calculation!" );
		break;
	}

	return data_size;
}

void Image::Allocate2D( VmaAllocator allocator, VkFormat format, VkExtent3D extent, VkImageUsageFlags usage, uint32_t mip_levels, VkImage* image, VmaAllocation* allocation ) {
	const VkImageCreateInfo create_info = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.pNext = nullptr,

		.imageType = VK_IMAGE_TYPE_2D,
		.format = format,
		.extent = extent,

		.mipLevels = mip_levels,
		.arrayLayers = 1,

		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = usage
	};

	const VmaAllocationCreateInfo alloc_info = {
		.usage = VMA_MEMORY_USAGE_GPU_ONLY,
		.requiredFlags = VkMemoryPropertyFlags( VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT )
	};
	VK_CHECK( vmaCreateImage( allocator, &create_info, &alloc_info, image, allocation, nullptr ) );
}

void Image::AllocateCubemap( VmaAllocator allocator, VkFormat format, VkExtent3D extent, VkImageUsageFlags usage, uint32_t mip_levels, VkImage* image, VmaAllocation* allocation ) {
	const VkImageCreateInfo create_info = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.pNext = nullptr,

		.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = format,
		.extent = extent,

		.mipLevels = mip_levels,
		.arrayLayers = 6,

		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = usage
	};

	const VmaAllocationCreateInfo alloc_info = {
		.usage = VMA_MEMORY_USAGE_GPU_ONLY,
		.requiredFlags = VkMemoryPropertyFlags( VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT )
	};
	VK_CHECK( vmaCreateImage( allocator, &create_info, &alloc_info, image, allocation, nullptr ) );
}

VkImageView Image::CreateView2D( VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspect_flags, uint32_t mip_level ) {
	VkImageView view;

	const VkImageViewCreateInfo view_info = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.pNext = nullptr,

		.image = image,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = format,
		.subresourceRange = {
			.aspectMask = aspect_flags,
			.baseMipLevel = mip_level ,
			.levelCount = VK_REMAINING_MIP_LEVELS,
			.baseArrayLayer = 0,
			.layerCount = 1,
		}
	};
	VK_CHECK( vkCreateImageView( device, &view_info, nullptr, &view ) );

	return view;
}

VkImageView Image::CreateViewCubemap( VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspect_flags, uint32_t mip_level ) {
	VkImageView view;

	const VkImageViewCreateInfo view_info = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.pNext = nullptr,

		.image = image,
		.viewType = VK_IMAGE_VIEW_TYPE_CUBE,
		.format = format,
		.subresourceRange = {
			.aspectMask = aspect_flags,
			.baseMipLevel = mip_level ,
			.levelCount = VK_REMAINING_MIP_LEVELS,
			.baseArrayLayer = 0,
			.layerCount = 6,
		}
	};
	VK_CHECK( vkCreateImageView( device, &view_info, nullptr, &view ) );

	return view;

}

void Image::TransitionLayout( VkCommandBuffer cmd, VkImage image, VkImageLayout current_layout, VkImageLayout new_layout, bool depth ) {
	assert( image != VK_NULL_HANDLE && "Transition on uninitialized image" );

	const VkImageMemoryBarrier2 image_barrier = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.pNext = nullptr,
		.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
		.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
		.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,

		.oldLayout = current_layout,
		.newLayout = new_layout,

		.image = image,

		.subresourceRange = {
			.aspectMask = VkImageAspectFlags( (depth) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT ),
			.baseMipLevel = 0,
			.levelCount = VK_REMAINING_MIP_LEVELS,
			.baseArrayLayer = 0,
			.layerCount = VK_REMAINING_ARRAY_LAYERS
		}
	};

	const VkDependencyInfo dependency_info = {
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.pNext = nullptr,
		.imageMemoryBarrierCount = 1,
		.pImageMemoryBarriers = &image_barrier
	};

	vkCmdPipelineBarrier2( cmd, &dependency_info );
}

void Image::GenerateMipmaps( VkCommandBuffer cmd, VkImage image, VkExtent2D image_size ) {
	const int mip_levels = int( std::floor( std::log2( std::max( image_size.width, image_size.height ) ) ) ) + 1;

	// copy from 0->1, 1->2, 2->3, etc...

	for ( uint32_t mip = 0; mip < mip_levels; mip++ ) {
		VkExtent2D half_size = image_size;
		half_size.width /= 2;
		half_size.height /= 2;

		// ---------
		// barrier around image for write access
		const VkImageMemoryBarrier2 image_barrier = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.pNext = nullptr,

			.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
			.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
			.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,

			.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,

			.image = image,

			.subresourceRange = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = mip,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = VK_REMAINING_ARRAY_LAYERS,
			}
		};

		const VkDependencyInfo dependency_info = {
			.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
			.pNext = nullptr,
			.imageMemoryBarrierCount = 1,
			.pImageMemoryBarriers = &image_barrier
		};

		vkCmdPipelineBarrier2( cmd, &dependency_info );

		// stop at last mip, since we cant copy the last one to anywhere
		if ( mip < mip_levels - 1 ) {
			const VkImageBlit2 blit_region = {
				.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
				.pNext = nullptr,

				.srcSubresource = {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.mipLevel = mip,
					.baseArrayLayer = 0,
					.layerCount = 1,
				},
				.srcOffsets = {
					{}, { (int32_t)image_size.width, (int32_t)image_size.height, 1 }
				},

				.dstSubresource = {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.mipLevel = mip + 1,
					.baseArrayLayer = 0,
					.layerCount = 1,
				},
				.dstOffsets = {
					{}, { (int32_t)half_size.width, (int32_t)half_size.height, 1 }
				},
			};

			const VkBlitImageInfo2 blit_info = {
				.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
				.pNext = nullptr,

				.srcImage = image,
				.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,

				.dstImage = image,
				.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,

				.regionCount = 1,
				.pRegions = &blit_region,
			};

			vkCmdBlitImage2( cmd, &blit_info );

			image_size = half_size;
		}
	}
}

void Image::CopyFromBuffer( VkCommandBuffer cmd, VkBuffer buffer, VkImage image, VkExtent3D extent, VkImageAspectFlags aspect, VkDeviceSize offset, uint32_t face ) {
	const VkBufferImageCopy copy_region = {
		.bufferOffset = offset,
		.imageSubresource = {
			.aspectMask = aspect,
			.mipLevel = 0,
			.baseArrayLayer = face,
			.layerCount = 1
		},
		.imageExtent = extent
	};

	vkCmdCopyBufferToImage( cmd, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region );
}
