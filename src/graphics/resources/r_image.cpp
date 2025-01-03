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

#include "r_image.h"

#include <graphics/resources/r_resources.h>
#include <graphics/utils/vk_types.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

TL::renderer::RImage::RImage( TL_VkContext* gfx, const std::string& name, void* data, const VkExtent3D extent,
                              const VkFormat format, const ImageType imageType, const VkImageUsageFlags usage,
                              bool generateMipmaps ) :
    m_gfx( gfx ), m_name( name ), m_extent( extent ), m_format( format ), m_usage( usage ),
    m_mipmapped( generateMipmaps ) {

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

TL::renderer::RImage::RImage( TL_VkContext* gfx, const std::string& name, VkExtent3D extent, VkFormat format, ImageType imageType,
                              VkImageUsageFlags usage, bool generateMipmaps ) :
    m_gfx( gfx ), m_name( name ), m_extent( extent ), m_format( format ), m_usage( usage ),
    m_mipmapped( generateMipmaps ) {

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

TL::renderer::RImage::~RImage( ) {
    if ( !m_gfx ) {
        return;
    }

    vkDestroyImageView( m_gfx->device, GetBaseView( ), nullptr );

    for ( const auto view : GetMipViews( ) ) {
        vkDestroyImageView( m_gfx->device, view, nullptr );
    }

    vmaDestroyImage( m_gfx->allocator, GetImage( ), GetAllocation( ) );
}

void TL::renderer::RImage::Resize( VkExtent3D size ) {
    assert( size.width > 0 && size.height > 0 );

    // backup old needed data
    VkImage                  original_image      = m_image;
    VkImageView              original_view       = m_view;
    std::vector<VkImageView> original_mip_views  = m_mipViews;
    VkExtent3D               original_extent     = m_extent;
    VmaAllocation            original_allocation = m_allocation;

    m_extent = size;

    if ( m_type == T2D ) {
        ActuallyCreateEmptyImage2D( );

        const bool depth = m_format == VK_FORMAT_D32_SFLOAT;
        m_gfx->Execute( [&]( VkCommandBuffer cmd ) {
            image::TransitionLayout( cmd, original_image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                     VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, depth );
            image::TransitionLayout( cmd, m_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, depth );
            image::Blit( cmd, original_image, { original_extent.width, original_extent.height }, m_image,
                         { m_extent.width, m_extent.height } );

            if ( m_mipmapped ) {
                GenerateMipmaps( cmd );
                TransitionLayout( cmd, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, depth );
            }
            else {
                TransitionLayout( cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, depth );
            }
        } );

        m_gfx->ImageCodex.bindlessRegistry.AddImage( *m_gfx, m_id, m_view );
    }

    // destroy old stuff
    vkDestroyImageView( m_gfx->device, original_view, nullptr );

    for ( const auto view : original_mip_views ) {
        vkDestroyImageView( m_gfx->device, view, nullptr );
    }

    vmaDestroyImage( m_gfx->allocator, original_image, original_allocation );
}

void TL::renderer::RImage::TransitionLayout( VkCommandBuffer cmd, VkImageLayout currentLayout, VkImageLayout newLayout, bool depth ) const {
    image::TransitionLayout( cmd, m_image, currentLayout, newLayout, depth );
}

void TL::renderer::RImage::GenerateMipmaps( VkCommandBuffer cmd ) const {
    image::GenerateMipmaps( cmd, m_image, VkExtent2D{ m_extent.width, m_extent.height } );
}

void TL::renderer::RImage::SetDebugName( const std::string& name ) {
    const VkDebugUtilsObjectNameInfoEXT obj = {
            .sType        = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
            .pNext        = nullptr,
            .objectType   = VK_OBJECT_TYPE_IMAGE,
            .objectHandle = reinterpret_cast<uint64_t>( m_image ),
            .pObjectName  = name.c_str( ),
    };
    vkSetDebugUtilsObjectNameEXT( m_gfx->device, &obj );
}

void TL::renderer::RImage::ActuallyCreateEmptyImage2D( ) {
    const bool               depth  = m_format == VK_FORMAT_D32_SFLOAT;
    const VkImageAspectFlags aspect = depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

    uint32_t mip_levels = 1;
    if ( m_mipmapped ) {
        mip_levels =
                static_cast<uint32_t>( std::floor( std::log2( std::max( m_extent.width, m_extent.height ) ) ) ) + 1;
    }

    // allocate vulkan image
    image::Allocate2D( m_gfx->allocator, m_format, m_extent, this->m_usage, mip_levels, &m_image, &m_allocation );

    SetDebugName( m_name );

    m_gfx->Execute( [&]( VkCommandBuffer cmd ) {
        VkImageLayout final_layout =
                depth ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        TransitionLayout( cmd, VK_IMAGE_LAYOUT_UNDEFINED, final_layout, depth );
    } );

    // create view
    m_view = image::CreateView2D( m_gfx->device, m_image, m_format, aspect );

    m_type = T2D;
}

void TL::renderer::RImage::ActuallyCreateImage2DFromData( const void* data ) {
    const size_t             data_size = image::CalculateSize( m_extent, m_format );
    const bool               depth     = m_format == VK_FORMAT_D32_SFLOAT;
    const VkImageAspectFlags aspect    = depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

    uint32_t mip_levels = 1;
    if ( m_mipmapped ) {
        mip_levels =
                static_cast<uint32_t>( std::floor( std::log2( std::max( m_extent.width, m_extent.height ) ) ) ) + 1;
    }

    // ----------
    // allocate vulkan image
    image::Allocate2D( m_gfx->allocator, m_format, m_extent, this->m_usage, mip_levels, &m_image, &m_allocation );

    SetDebugName( m_name );

    // upload buffer to image & generate mipmaps if needed
    const auto staging_buffer = TL::Buffer( TL::BufferType::TStaging, data_size, 1, nullptr, __FUNCTION__ );
    staging_buffer.Upload( data, data_size );

    m_gfx->Execute( [&]( VkCommandBuffer cmd ) {
        TransitionLayout( cmd, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, depth );

        image::CopyFromBuffer( cmd, staging_buffer.GetVkResource( ), m_image, m_extent, aspect );

        if ( m_mipmapped ) {
            GenerateMipmaps( cmd );
            TransitionLayout( cmd, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, depth );
        }
        else {
            TransitionLayout( cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, depth );
        }
    } );

    // create views
    m_view = image::CreateView2D( m_gfx->device, m_image, m_format, aspect );

    m_type = T2D;
}

void TL::renderer::RImage::ActuallyCreateCubemapFromData( const void* data ) {
    const size_t face_size = image::CalculateSize( m_extent, m_format );
    const size_t data_size = face_size * 6;
    const bool   depth     = m_format == VK_FORMAT_D32_SFLOAT;

    VkImageAspectFlags aspect = depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

    uint32_t mip_levels = 1;
    if ( m_mipmapped ) {
        mip_levels =
                static_cast<uint32_t>( std::floor( std::log2( std::max( m_extent.width, m_extent.height ) ) ) ) + 1;
    }

    image::AllocateCubemap( m_gfx->allocator, m_format, m_extent, this->m_usage, mip_levels, &m_image, &m_allocation );

    SetDebugName( m_name );

    // upload data & generate mipmaps
    const auto staging_buffer = TL::Buffer( TL::BufferType::TStaging, data_size, 1, nullptr, __FUNCTION__ );
    staging_buffer.Upload( data, data_size );

    m_gfx->Execute( [&]( VkCommandBuffer cmd ) {
        TransitionLayout( cmd, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, depth );

        for ( uint32_t face = 0; face < 6; face++ ) {
            size_t offset = face * face_size;

            image::CopyFromBuffer( cmd, staging_buffer.GetVkResource( ), m_image, m_extent, aspect, offset, face );
        }

        if ( m_mipmapped ) {
            GenerateMipmaps( cmd );
            TransitionLayout( cmd, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, depth );
        }
        else {
            TransitionLayout( cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, depth );
        }
    } );

    // create views
    for ( u32 i = 0; i < mip_levels; i++ ) {
        m_mipViews.push_back( image::CreateViewCubemap( m_gfx->device, m_image, m_format, aspect, i ) );
    }

    m_view = image::CreateViewCubemap( m_gfx->device, m_image, m_format, aspect );

    m_type = TCubeMap;
}

void TL::renderer::RImage::ActuallyCreateEmptyCubemap( ) {
    const bool depth = m_format == VK_FORMAT_D32_SFLOAT;

    const VkImageAspectFlags aspect = depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

    uint32_t mip_levels = 1;
    if ( m_mipmapped ) {
        mip_levels =
                static_cast<uint32_t>( std::floor( std::log2( std::max( m_extent.width, m_extent.height ) ) ) ) + 1;
    }

    image::AllocateCubemap( m_gfx->allocator, m_format, m_extent, this->m_usage, mip_levels, &m_image, &m_allocation );

    SetDebugName( m_name );

    // upload data & generate mipmaps
    m_gfx->Execute( [&]( VkCommandBuffer cmd ) {
        TransitionLayout( cmd, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, depth );

        if ( m_mipmapped ) {
            GenerateMipmaps( cmd );
            TransitionLayout( cmd, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, depth );
        }
        else {
            TransitionLayout( cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, depth );
        }
    } );

    // create views
    for ( u32 i = 0; i < mip_levels; i++ ) {
        m_mipViews.push_back( image::CreateViewCubemap( m_gfx->device, m_image, m_format, aspect, i ) );
    }

    m_view = image::CreateViewCubemap( m_gfx->device, m_image, m_format, aspect );

    m_type = TCubeMap;
}


void TL::renderer::ImageCodex::Init( TL_VkContext* gfx ) {
    this->m_gfx = gfx;
    bindlessRegistry.Init( *this->m_gfx );

    InitDefaultImages( );
}

void TL::renderer::ImageCodex::Cleanup( ) {
    m_images.clear( );
    bindlessRegistry.Cleanup( *m_gfx );
}

const std::vector<TL::renderer::RImage>& TL::renderer::ImageCodex::GetImages( ) { return m_images; }

TL::renderer::RImage& TL::renderer::ImageCodex::GetImage( const ImageId id ) { return m_images[id]; }

ImageId TL::renderer::ImageCodex::LoadImageFromFile( const std::string& path, const VkFormat format, const VkImageUsageFlags usage,
                                                     const bool mipmapped ) {
    // search for already loaded image
    for ( const auto& img : m_images ) {
        if ( path == img.GetName( ) && format == img.GetFormat( ) && usage == img.GetUsage( ) &&
             mipmapped == img.IsMipmapped( ) ) {
            return img.GetId( );
        }
    }

    int            width, height, nr_channels;
    unsigned char* data = stbi_load( path.c_str( ), &width, &height, &nr_channels, 4 );
    if ( !data ) {
        return -1;
    }

    const VkExtent3D extent = {
            .width  = static_cast<uint32_t>( width ),
            .height = static_cast<uint32_t>( height ),
            .depth  = 1,
    };

    const auto id = LoadImageFromData( path, data, extent, format, usage, mipmapped );

    stbi_image_free( data );

    return id;
}

ImageId TL::renderer::ImageCodex::LoadHdrFromFile( const std::string& path, const VkFormat format, const VkImageUsageFlags usage,
                                                   const bool mipmapped ) {
    // search for already loaded image
    for ( const auto& img : m_images ) {
        if ( path == img.GetName( ) && format == img.GetFormat( ) && usage == img.GetUsage( ) &&
             mipmapped == img.IsMipmapped( ) ) {
            return img.GetId( );
        }
    }

    int    width, height, nr_channels;
    float* data = stbi_loadf( path.c_str( ), &width, &height, &nr_channels, 4 );
    if ( !data ) {
        return -1;
    }

    const VkExtent3D extent = {
            .width  = static_cast<uint32_t>( width ),
            .height = static_cast<uint32_t>( height ),
            .depth  = 1,
    };

    const auto id = LoadImageFromData( path, data, extent, format, usage, mipmapped );

    stbi_image_free( data );

    return id;
}

ImageId TL::renderer::ImageCodex::LoadCubemapFromFile( const std::vector<std::string>& paths, const VkFormat format,
                                                       const VkImageUsageFlags usage, const bool mipmapped ) {
    assert( paths.size( ) == 6 && "Cubemap needs 6 faces!" );

    for ( const auto& img : m_images ) {
        if ( paths[0] == img.GetName( ) && format == img.GetFormat( ) && usage == img.GetUsage( ) &&
             mipmapped == img.IsMipmapped( ) ) {
            return img.GetId( );
        }
    }

    VkExtent3D extent = {
            .width  = 0,
            .height = 0,
            .depth  = 1,
    };

    stbi_set_flip_vertically_on_load( true );
    std::vector<unsigned char*> datas;
    for ( size_t i = 0; i < 6; i++ ) {
        auto& path = paths.at( i );

        int            width, height, nr_channels;
        unsigned char* data = stbi_load( path.c_str( ), &width, &height, &nr_channels, 4 );
        if ( !data ) {
            return -1;
        }

        extent.width  = width;
        extent.height = height;

        datas.push_back( data );
    }

    const auto image_id = LoadCubemapFromData( paths, datas, extent, format, usage, mipmapped );
    stbi_set_flip_vertically_on_load( false );

    for ( const auto data : datas ) {
        stbi_image_free( data );
    }

    return image_id;
}

ImageId TL::renderer::ImageCodex::LoadCubemapFromData( const std::vector<std::string>&    paths,
                                                       const std::vector<unsigned char*>& datas, const VkExtent3D extent,
                                                       const VkFormat format, const VkImageUsageFlags usage, const bool mipmapped ) {
    const size_t total_size = image::CalculateSize( extent, format ) * datas.size( );
    const size_t face_size  = image::CalculateSize( extent, format );

    unsigned char* merged_data = new unsigned char[total_size];
    size_t         offset      = { };

    for ( const auto& data : datas ) {
        std::memcpy( merged_data + offset, data, face_size );
        offset += face_size;
    }

    auto image = RImage( m_gfx, paths.at( 0 ), ( void* )merged_data, extent, format, ImageType::TCubeMap, usage,
                         mipmapped );

    delete[] merged_data;

    const ImageId image_id = GetAvailableId( );
    image.SetId( image_id );

    bindlessRegistry.AddImage( *m_gfx, image_id, image.GetBaseView( ) );
    m_images[image_id] = std::move( image );

    return image_id;
}

ImageId TL::renderer::ImageCodex::CreateCubemap( const std::string& name, const VkExtent3D extent, const VkFormat format,
                                                 const VkImageUsageFlags usage, int mipmaps ) {
    auto image = RImage( m_gfx, name, extent, format, ImageType::TCubeMap, usage, mipmaps );

    const ImageId image_id = GetAvailableId( );
    image.SetId( image_id );

    bindlessRegistry.AddImage( *m_gfx, image_id, image.GetBaseView( ) );
    m_images[image_id] = std::move( image );
    return image_id;
}

ImageId TL::renderer::ImageCodex::CreateEmptyImage( const std::string& name, const VkExtent3D extent, const VkFormat format,
                                                    const VkImageUsageFlags usage, const bool mipmapped ) {
    auto image = RImage( m_gfx, name, extent, format, ImageType::T2D, usage, mipmapped );

    const ImageId image_id = GetAvailableId( );
    image.SetId( image_id );

    bindlessRegistry.AddImage( *m_gfx, image_id, image.GetBaseView( ) );
    m_images[image_id] = std::move( image );
    return image_id;
}

ImageId TL::renderer::ImageCodex::LoadImageFromData( const std::string& name, void* data, const VkExtent3D extent,
                                                     const VkFormat format, const VkImageUsageFlags usage, const bool mipmapped ) {
    auto image = RImage( m_gfx, name, data, extent, format, ImageType::T2D, usage, mipmapped );

    const ImageId image_id = GetAvailableId( );
    image.SetId( image_id );

    bindlessRegistry.AddImage( *m_gfx, image_id, image.GetBaseView( ) );
    m_images[image_id] = std::move( image );

    return image_id;
}

VkDescriptorSetLayout TL::renderer::ImageCodex::GetBindlessLayout( ) const { return bindlessRegistry.layout; }

VkDescriptorSet TL::renderer::ImageCodex::GetBindlessSet( ) const { return bindlessRegistry.set; }

void TL::renderer::ImageCodex::UnloadIamge( const ImageId id ) {
    auto image = std::move( GetImage( id ) );
    m_freeIds.push_back( id );
}

void TL::renderer::ImageCodex::DrawDebug( ) const {
    ImGui::Columns( 10 );
    {
        for ( u64 i = 1; i < m_images.size( ); i++ ) {
            auto& image        = m_images.at( i );
            f32   column_width = ImGui::GetColumnWidth( );
            ImGui::Image( ( ImTextureID )( i ), ImVec2( column_width, column_width ) );
            if ( ImGui::IsItemHovered( ) ) {
                ImGui::BeginTooltip( );
                ImGui::Text( "%s", image.GetName( ).c_str( ) );
                ImGui::Separator( );
                ImGui::Image( ( ImTextureID )( i ),
                              ImVec2( ( f32 )image.GetExtent( ).width, ( f32 )image.GetExtent( ).height ) );
                ImGui::EndTooltip( );
            }
            ImGui::NextColumn( );
        }
    }
    ImGui::Columns( 1 );
}

void TL::renderer::ImageCodex::InitDefaultImages( ) {
    // 3 default textures, white, grey, black. 1 pixel each
    uint32_t white = glm::packUnorm4x8( glm::vec4( 1, 1, 1, 1 ) );
    white          = LoadImageFromData( "debug_white_img", ( void* )&white, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
                                        VK_IMAGE_USAGE_SAMPLED_BIT, false );

    uint32_t grey = glm::packUnorm4x8( glm::vec4( 0.66f, 0.66f, 0.66f, 1 ) );
    grey          = LoadImageFromData( "debug_grey_img", ( void* )&grey, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
                                       VK_IMAGE_USAGE_SAMPLED_BIT, false );

    uint32_t black = glm::packUnorm4x8( glm::vec4( 0, 0, 0, 0 ) );
    black          = LoadImageFromData( "debug_black_img", ( void* )&white, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
                                        VK_IMAGE_USAGE_SAMPLED_BIT, false );

    // checkerboard image
    const uint32_t                magenta = glm::packUnorm4x8( glm::vec4( 1, 0, 1, 1 ) );
    std::array<uint32_t, 16 * 16> pixels; // for 16x16 checkerboard texture
    for ( int x = 0; x < 16; x++ ) {
        for ( int y = 0; y < 16; y++ ) {
            pixels[y * 16 + x] = ( ( x % 2 ) ^ ( y % 2 ) ) ? magenta : black;
        }
    }
    m_checkboard = LoadImageFromData( "debug_checkboard_img", ( void* )&white, VkExtent3D{ 16, 16, 1 },
                                      VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, false );
}

ImageId TL::renderer::ImageCodex::GetAvailableId( ) {
    if ( m_freeIds.empty( ) ) {
        const auto id = m_images.size( );
        m_images.resize( id + 1 );
        return id;
    }

    const auto id = m_freeIds.back( );
    m_freeIds.pop_back( );

    return id;
}

size_t TL::renderer::image::CalculateSize( VkExtent3D extent, VkFormat format ) {
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

void TL::renderer::image::Allocate2D( VmaAllocator allocator, VkFormat format, VkExtent3D extent, VkImageUsageFlags usage,
                                      uint32_t mipLevels, VkImage* image, VmaAllocation* allocation ) {
    const VkImageCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = nullptr,

            .imageType = VK_IMAGE_TYPE_2D,
            .format    = format,
            .extent    = extent,

            .mipLevels   = mipLevels,
            .arrayLayers = 1,

            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling  = VK_IMAGE_TILING_OPTIMAL,
            .usage   = usage,
    };

    constexpr VmaAllocationCreateInfo alloc_info = {
            .usage         = VMA_MEMORY_USAGE_GPU_ONLY,
            .requiredFlags = static_cast<VkMemoryPropertyFlags>( VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT ),
    };
    VKCALL( vmaCreateImage( allocator, &create_info, &alloc_info, image, allocation, nullptr ) );
}

void TL::renderer::image::AllocateCubemap( VmaAllocator allocator, VkFormat format, VkExtent3D extent, VkImageUsageFlags usage,
                                           uint32_t mipLevels, VkImage* image, VmaAllocation* allocation ) {
    const VkImageCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = nullptr,

            .flags     = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
            .imageType = VK_IMAGE_TYPE_2D,
            .format    = format,
            .extent    = extent,

            .mipLevels   = mipLevels,
            .arrayLayers = 6,

            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling  = VK_IMAGE_TILING_OPTIMAL,
            .usage   = usage,
    };

    constexpr VmaAllocationCreateInfo alloc_info = {
            .usage         = VMA_MEMORY_USAGE_GPU_ONLY,
            .requiredFlags = static_cast<VkMemoryPropertyFlags>( VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT ),
    };
    VKCALL( vmaCreateImage( allocator, &create_info, &alloc_info, image, allocation, nullptr ) );
}

VkImageView TL::renderer::image::CreateView2D( VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags,
                                               uint32_t mipLevel ) {
    VkImageView view;

    const VkImageViewCreateInfo view_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = nullptr,

            .image    = image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format   = format,
            .subresourceRange =
                    {
                            .aspectMask     = aspectFlags,
                            .baseMipLevel   = mipLevel,
                            .levelCount     = VK_REMAINING_MIP_LEVELS,
                            .baseArrayLayer = 0,
                            .layerCount     = 1,
                    },
    };
    VKCALL( vkCreateImageView( device, &view_info, nullptr, &view ) );

    return view;
}

VkImageView TL::renderer::image::CreateViewCubemap( VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags,
                                                    uint32_t mipLevel ) {
    VkImageView view;

    const VkImageViewCreateInfo view_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = nullptr,

            .image    = image,
            .viewType = VK_IMAGE_VIEW_TYPE_CUBE,
            .format   = format,
            .subresourceRange =
                    {
                            .aspectMask     = aspectFlags,
                            .baseMipLevel   = mipLevel,
                            .levelCount     = VK_REMAINING_MIP_LEVELS,
                            .baseArrayLayer = 0,
                            .layerCount     = 6,
                    },
    };
    VKCALL( vkCreateImageView( device, &view_info, nullptr, &view ) );

    return view;
}

void TL::renderer::image::TransitionLayout( VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout, bool depth ) {
    assert( image != VK_NULL_HANDLE && "Transition on uninitialized image" );

    VkImageMemoryBarrier2 image_barrier = {
            .sType         = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .pNext         = nullptr,
            .srcStageMask  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
            .dstStageMask  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            .dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,

            .oldLayout = currentLayout,
            .newLayout = newLayout,

            .image = image,

            .subresourceRange = {
                    .aspectMask     = static_cast<VkImageAspectFlags>( ( depth ) ? VK_IMAGE_ASPECT_DEPTH_BIT
                                                                                 : VK_IMAGE_ASPECT_COLOR_BIT ),
                    .baseMipLevel   = 0,
                    .levelCount     = VK_REMAINING_MIP_LEVELS,
                    .baseArrayLayer = 0,
                    .layerCount     = VK_REMAINING_ARRAY_LAYERS },
    };

    // Set the source and destination access masks based on the current and new layouts
    if ( currentLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL ) {
        image_barrier.srcAccessMask = 0;
        image_barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    }
    else if ( currentLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ) {
        image_barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        image_barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
    }
    else if ( currentLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR ) {
        image_barrier.srcAccessMask = 0;
        image_barrier.dstAccessMask = 0;
    }
    else if ( newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL ) {
        image_barrier.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    }
    else {
        // Other layout transitions can be added here
        image_barrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
        image_barrier.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
    }

    const VkDependencyInfo dependency_info = {
            .sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .pNext                   = nullptr,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers    = &image_barrier,
    };

    vkCmdPipelineBarrier2( cmd, &dependency_info );
}

void TL::renderer::image::GenerateMipmaps( VkCommandBuffer cmd, VkImage image, VkExtent2D imageSize ) {
    const u32 mip_levels =
            static_cast<int>( std::floor( std::log2( std::max( imageSize.width, imageSize.height ) ) ) ) + 1;

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

                .srcStageMask  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
                .dstStageMask  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                .dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,

                .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,

                .image = image,

                .subresourceRange =
                        {
                                .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                                .baseMipLevel   = mip,
                                .levelCount     = 1,
                                .baseArrayLayer = 0,
                                .layerCount     = VK_REMAINING_ARRAY_LAYERS,
                        },
        };

        const VkDependencyInfo dependency_info = { .sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                                   .pNext                   = nullptr,
                                                   .imageMemoryBarrierCount = 1,
                                                   .pImageMemoryBarriers    = &image_barrier };

        vkCmdPipelineBarrier2( cmd, &dependency_info );

        // stop at last mip, since we cant copy the last one to anywhere
        if ( mip < mip_levels - 1 ) {
            const VkImageBlit2 blit_region = {
                    .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
                    .pNext = nullptr,

                    .srcSubresource =
                            {
                                    .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                                    .mipLevel       = mip,
                                    .baseArrayLayer = 0,
                                    .layerCount     = 1,
                            },
                    .srcOffsets = { { },
                                    { static_cast<int32_t>( imageSize.width ), static_cast<int32_t>( imageSize.height ),
                                      1 } },

                    .dstSubresource =
                            {
                                    .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                                    .mipLevel       = mip + 1,
                                    .baseArrayLayer = 0,
                                    .layerCount     = 1,
                            },
                    .dstOffsets = { { },
                                    { static_cast<int32_t>( half_size.width ), static_cast<int32_t>( half_size.height ),
                                      1 } },
            };

            const VkBlitImageInfo2 blit_info = {
                    .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
                    .pNext = nullptr,

                    .srcImage       = image,
                    .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,

                    .dstImage       = image,
                    .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,

                    .regionCount = 1,
                    .pRegions    = &blit_region,
            };

            vkCmdBlitImage2( cmd, &blit_info );

            imageSize = half_size;
        }
    }
}

void TL::renderer::image::CopyFromBuffer( VkCommandBuffer cmd, VkBuffer buffer, VkImage image, VkExtent3D extent,
                                          VkImageAspectFlags aspect, VkDeviceSize offset, uint32_t face ) {
    const VkBufferImageCopy copy_region = {
            .bufferOffset     = offset,
            .imageSubresource = { .aspectMask = aspect, .mipLevel = 0, .baseArrayLayer = face, .layerCount = 1 },
            .imageExtent      = extent,
    };

    vkCmdCopyBufferToImage( cmd, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region );
}

void TL::renderer::image::Blit( VkCommandBuffer cmd, VkImage srcImage, VkExtent2D srcExtent, VkImage dstImage, VkExtent2D dstExtent,
                                VkFilter filter ) {
    const VkImageBlit2 blit_region = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
            .pNext = nullptr,
            .srcSubresource =
                    {
                            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                            .mipLevel       = 0,
                            .baseArrayLayer = 0,
                            .layerCount     = 1,
                    },
            .srcOffsets =
                    {
                            { 0, 0, 0 },
                            { static_cast<int32_t>( srcExtent.width ), static_cast<int32_t>( srcExtent.height ), 1 },
                    },
            .dstSubresource =
                    {
                            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                            .mipLevel       = 0,
                            .baseArrayLayer = 0,
                            .layerCount     = 1,
                    },
            .dstOffsets =
                    {
                            { 0, 0, 0 },
                            { static_cast<int32_t>( dstExtent.width ), static_cast<int32_t>( dstExtent.height ), 1 },
                    },
    };

    const VkBlitImageInfo2 blit_info = {
            .sType          = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
            .pNext          = nullptr,
            .srcImage       = srcImage,
            .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .dstImage       = dstImage,
            .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .regionCount    = 1,
            .pRegions       = &blit_region,
            .filter         = filter,
    };

    vkCmdBlitImage2( cmd, &blit_info );
}
