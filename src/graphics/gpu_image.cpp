/******************************************************************************
******************************************************************************
**                                                                           **
**                             Twilight Engine                               **
**                                                                           **
**                  Copyright (c) 2024-present Daniel Dron                   **
**                                                                           **
**            This software is released under the MIT License.               **
**                 https://opensource.org/licenses/MIT                       **
**                                                                           **
******************************************************************************
******************************************************************************/

#include <pch.h>

#include "gpu_image.h"

#include <graphics/gfx_device.h>
#include <vk_types.h>

GpuImage::GpuImage( GfxDevice *gfx, const std::string &name, void *data, const VkExtent3D extent, const VkFormat format, const ImageType imageType, const VkImageUsageFlags usage, bool generateMipmaps ) :
    m_gfx( gfx ), m_name( name ), m_extent( extent ), m_format( format ), m_usage( usage ), m_mipmapped( generateMipmaps ) {

    assert( data != nullptr );
    assert( extent.width > 0 && extent.height > 0 );
    // assert( !name.empty( ) );

    this->m_usage = usage | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    if ( imageType == T2D ) {
        ActuallyCreateImage2DFromData( data );
    }
    else if ( imageType == TCubeMap ) {
        ActuallyCreateCubemapFromData( data );
    }
}

GpuImage::GpuImage( GfxDevice *gfx, const std::string &name, VkExtent3D extent, VkFormat format, ImageType imageType, VkImageUsageFlags usage, bool generateMipmaps ) :
    m_gfx( gfx ), m_name( name ), m_extent( extent ), m_format( format ), m_usage( usage ), m_mipmapped( generateMipmaps ) {

    assert( extent.width > 0 && extent.height > 0 );
    assert( !name.empty( ) );

    this->m_usage = usage | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    if ( imageType == T2D ) {
        ActuallyCreateEmptyImage2D( );
    }
    else if ( imageType == TCubeMap ) {
        ActuallyCreateEmptyCubemap( );
    }
}

GpuImage::~GpuImage( ) {
    if ( !m_gfx ) {
        return;
    }

    vkDestroyImageView( m_gfx->device, GetBaseView( ), nullptr );

    for ( const auto view : GetMipViews( ) ) {
        vkDestroyImageView( m_gfx->device, view, nullptr );
    }

    vmaDestroyImage( m_gfx->allocator, GetImage( ), GetAllocation( ) );
}

void GpuImage::TransitionLayout( VkCommandBuffer cmd, VkImageLayout currentLayout, VkImageLayout newLayout, bool depth ) const {
    image::TransitionLayout( cmd, m_image, currentLayout, newLayout, depth );
}

void GpuImage::GenerateMipmaps( VkCommandBuffer cmd ) const {
    image::GenerateMipmaps( cmd, m_image, VkExtent2D{ m_extent.width, m_extent.height } );
}

void GpuImage::SetDebugName( const std::string &name ) {
    const VkDebugUtilsObjectNameInfoEXT obj = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
            .pNext = nullptr,
            .objectType = VK_OBJECT_TYPE_IMAGE,
            .objectHandle = reinterpret_cast<uint64_t>( m_image ),
            .pObjectName = name.c_str( ),
    };
    vkSetDebugUtilsObjectNameEXT( m_gfx->device, &obj );
}

void GpuImage::ActuallyCreateEmptyImage2D( ) {
    const bool depth = m_format == VK_FORMAT_D32_SFLOAT;
    const VkImageAspectFlags aspect = depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

    uint32_t mip_levels = 1;
    if ( m_mipmapped ) {
        mip_levels = static_cast<uint32_t>( std::floor( std::log2( std::max( m_extent.width, m_extent.height ) ) ) ) + 1;
    }

    // allocate vulkan image
    image::Allocate2D( m_gfx->allocator, m_format, m_extent, this->m_usage, mip_levels, &m_image, &m_allocation );

    SetDebugName( m_name );

    m_gfx->Execute( [&]( VkCommandBuffer cmd ) {
        VkImageLayout final_layout = depth ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        TransitionLayout( cmd, VK_IMAGE_LAYOUT_UNDEFINED, final_layout, depth );
    } );

    // create view
    m_view = image::CreateView2D( m_gfx->device, m_image, m_format, aspect );

    m_type = T2D;
}

void GpuImage::ActuallyCreateImage2DFromData( const void *data ) {
    const size_t data_size = image::CalculateSize( m_extent, m_format );
    const bool depth = m_format == VK_FORMAT_D32_SFLOAT;
    const VkImageAspectFlags aspect = depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

    uint32_t mip_levels = 1;
    if ( m_mipmapped ) {
        mip_levels = static_cast<uint32_t>( std::floor( std::log2( std::max( m_extent.width, m_extent.height ) ) ) ) + 1;
    }

    // ----------
    // allocate vulkan image
    image::Allocate2D( m_gfx->allocator, m_format, m_extent, this->m_usage, mip_levels, &m_image, &m_allocation );

    SetDebugName( m_name );

    // upload buffer to image & generate mipmaps if needed
    GpuBuffer staging_buffer = m_gfx->Allocate( data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, __FUNCTION__ );
    staging_buffer.Upload( *m_gfx, data, data_size );

    m_gfx->Execute( [&]( VkCommandBuffer cmd ) {
        TransitionLayout( cmd, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, depth );

        image::CopyFromBuffer( cmd, staging_buffer.buffer, m_image, m_extent, aspect );

        if ( m_mipmapped ) {
            GenerateMipmaps( cmd );
            TransitionLayout( cmd, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL );
        }
        else {
            TransitionLayout( cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL );
        }
    } );
    m_gfx->Free( staging_buffer );

    // create views
    m_view = image::CreateView2D( m_gfx->device, m_image, m_format, aspect );

    m_type = T2D;
}

void GpuImage::ActuallyCreateCubemapFromData( const void *data ) {
    const size_t face_size = image::CalculateSize( m_extent, m_format );
    const size_t data_size = face_size * 6;
    const bool depth = m_format == VK_FORMAT_D32_SFLOAT;

    VkImageAspectFlags aspect = depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

    uint32_t mip_levels = 1;
    if ( m_mipmapped ) {
        mip_levels = static_cast<uint32_t>( std::floor( std::log2( std::max( m_extent.width, m_extent.height ) ) ) ) + 1;
    }

    image::AllocateCubemap( m_gfx->allocator, m_format, m_extent, this->m_usage, mip_levels, &m_image, &m_allocation );

    SetDebugName( m_name );

    // upload data & generate mipmaps
    const GpuBuffer staging_buffer = m_gfx->Allocate( data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, __FUNCTION__ );
    staging_buffer.Upload( *m_gfx, data, data_size );

    m_gfx->Execute( [&]( VkCommandBuffer cmd ) {
        TransitionLayout( cmd, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, depth );

        for ( uint32_t face = 0; face < 6; face++ ) {
            size_t offset = face * face_size;

            image::CopyFromBuffer( cmd, staging_buffer.buffer, m_image, m_extent, aspect, offset, face );
        }

        if ( m_mipmapped ) {
            GenerateMipmaps( cmd );
            TransitionLayout( cmd, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL );
        }
        else {
            TransitionLayout( cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL );
        }
    } );
    m_gfx->Free( staging_buffer );

    // create views
    for ( int i = 0; i < mip_levels; i++ ) {
        m_mipViews.push_back( image::CreateViewCubemap( m_gfx->device, m_image, m_format, aspect, i ) );
    }

    m_view = image::CreateViewCubemap( m_gfx->device, m_image, m_format, aspect );

    m_type = TCubeMap;
}

void GpuImage::ActuallyCreateEmptyCubemap( ) {
    const size_t face_size = image::CalculateSize( m_extent, m_format );
    const bool depth = m_format == VK_FORMAT_D32_SFLOAT;

    const VkImageAspectFlags aspect = depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

    uint32_t mip_levels = 1;
    if ( m_mipmapped ) {
        mip_levels = static_cast<uint32_t>( std::floor( std::log2( std::max( m_extent.width, m_extent.height ) ) ) ) + 1;
    }

    image::AllocateCubemap( m_gfx->allocator, m_format, m_extent, this->m_usage, mip_levels, &m_image, &m_allocation );

    SetDebugName( m_name );

    // upload data & generate mipmaps
    m_gfx->Execute( [&]( VkCommandBuffer cmd ) {
        TransitionLayout( cmd, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, depth );

        if ( m_mipmapped ) {
            GenerateMipmaps( cmd );
            TransitionLayout( cmd, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL );
        }
        else {
            TransitionLayout( cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL );
        }
    } );

    // create views
    for ( int i = 0; i < mip_levels; i++ ) {
        m_mipViews.push_back( image::CreateViewCubemap( m_gfx->device, m_image, m_format, aspect, i ) );
    }

    m_view = image::CreateViewCubemap( m_gfx->device, m_image, m_format, aspect );

    m_type = TCubeMap;
}

size_t image::CalculateSize( VkExtent3D extent, VkFormat format ) {
    size_t data_size = static_cast<size_t>( extent.width ) * extent.height * extent.depth;

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

void image::Allocate2D( VmaAllocator allocator, VkFormat format, VkExtent3D extent, VkImageUsageFlags usage, uint32_t mipLevels, VkImage *image, VmaAllocation *allocation ) {
    const VkImageCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = nullptr,

            .imageType = VK_IMAGE_TYPE_2D,
            .format = format,
            .extent = extent,

            .mipLevels = mipLevels,
            .arrayLayers = 1,

            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = usage,
    };

    constexpr VmaAllocationCreateInfo alloc_info = {
            .usage = VMA_MEMORY_USAGE_GPU_ONLY,
            .requiredFlags = static_cast<VkMemoryPropertyFlags>( VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT ),
    };
    VK_CHECK( vmaCreateImage( allocator, &create_info, &alloc_info, image, allocation, nullptr ) );
}

void image::AllocateCubemap( VmaAllocator allocator, VkFormat format, VkExtent3D extent, VkImageUsageFlags usage, uint32_t mipLevels, VkImage *image, VmaAllocation *allocation ) {
    const VkImageCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = nullptr,

            .flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = format,
            .extent = extent,

            .mipLevels = mipLevels,
            .arrayLayers = 6,

            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = usage,
    };

    constexpr VmaAllocationCreateInfo alloc_info = {
            .usage = VMA_MEMORY_USAGE_GPU_ONLY,
            .requiredFlags = static_cast<VkMemoryPropertyFlags>( VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT ),
    };
    VK_CHECK( vmaCreateImage( allocator, &create_info, &alloc_info, image, allocation, nullptr ) );
}

VkImageView image::CreateView2D( VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevel ) {
    VkImageView view;

    const VkImageViewCreateInfo view_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = nullptr,

            .image = image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = format,
            .subresourceRange = {
                    .aspectMask = aspectFlags,
                    .baseMipLevel = mipLevel,
                    .levelCount = VK_REMAINING_MIP_LEVELS,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
            },
    };
    VK_CHECK( vkCreateImageView( device, &view_info, nullptr, &view ) );

    return view;
}

VkImageView image::CreateViewCubemap( VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevel ) {
    VkImageView view;

    const VkImageViewCreateInfo view_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = nullptr,

            .image = image,
            .viewType = VK_IMAGE_VIEW_TYPE_CUBE,
            .format = format,
            .subresourceRange = {
                    .aspectMask = aspectFlags,
                    .baseMipLevel = mipLevel,
                    .levelCount = VK_REMAINING_MIP_LEVELS,
                    .baseArrayLayer = 0,
                    .layerCount = 6,
            },
    };
    VK_CHECK( vkCreateImageView( device, &view_info, nullptr, &view ) );

    return view;
}

void image::TransitionLayout( VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout, bool depth ) {
    assert( image != VK_NULL_HANDLE && "Transition on uninitialized image" );

    const VkImageMemoryBarrier2 image_barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .pNext = nullptr,
            .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            .dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,

            .oldLayout = currentLayout,
            .newLayout = newLayout,

            .image = image,

            .subresourceRange = {
                    .aspectMask = static_cast<VkImageAspectFlags>( ( depth ) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT ),
                    .baseMipLevel = 0,
                    .levelCount = VK_REMAINING_MIP_LEVELS,
                    .baseArrayLayer = 0,
                    .layerCount = VK_REMAINING_ARRAY_LAYERS,
            },
    };

    const VkDependencyInfo dependency_info = {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .pNext = nullptr,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &image_barrier,
    };

    vkCmdPipelineBarrier2( cmd, &dependency_info );
}

void image::GenerateMipmaps( VkCommandBuffer cmd, VkImage image, VkExtent2D imageSize ) {
    const int mip_levels = static_cast<int>( std::floor( std::log2( std::max( imageSize.width, imageSize.height ) ) ) ) + 1;

    // copy from 0->1, 1->2, 2->3, etc...

    for ( uint32_t mip = 0; mip < mip_levels; mip++ ) {
        VkExtent2D half_size = imageSize;
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
                },
        };

        const VkDependencyInfo dependency_info = {
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .pNext = nullptr,
                .imageMemoryBarrierCount = 1,
                .pImageMemoryBarriers = &image_barrier };

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
                    .srcOffsets = { { }, { static_cast<int32_t>( imageSize.width ), static_cast<int32_t>( imageSize.height ), 1 } },

                    .dstSubresource = {
                            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                            .mipLevel = mip + 1,
                            .baseArrayLayer = 0,
                            .layerCount = 1,
                    },
                    .dstOffsets = { { }, { static_cast<int32_t>( half_size.width ), static_cast<int32_t>( half_size.height ), 1 } },
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

            imageSize = half_size;
        }
    }
}

void image::CopyFromBuffer( VkCommandBuffer cmd, VkBuffer buffer, VkImage image, VkExtent3D extent, VkImageAspectFlags aspect, VkDeviceSize offset, uint32_t face ) {
    const VkBufferImageCopy copy_region = {
            .bufferOffset = offset,
            .imageSubresource = {
                    .aspectMask = aspect,
                    .mipLevel = 0,
                    .baseArrayLayer = face,
                    .layerCount = 1 },
            .imageExtent = extent,
    };

    vkCmdCopyBufferToImage( cmd, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region );
}
