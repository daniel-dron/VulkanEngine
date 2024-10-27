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

#include "image_codex.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include "tl_vkcontext.h"

#include <imgui.h>

void ImageCodex::Init( TL_VkContext *gfx ) {
    this->m_gfx = gfx;
    bindlessRegistry.Init( *this->m_gfx );

    InitDefaultImages( );
}

void ImageCodex::Cleanup( ) {
    m_images.clear( );
    bindlessRegistry.Cleanup( *m_gfx );
}

const std::vector<GpuImage> &ImageCodex::GetImages( ) { return m_images; }

GpuImage &ImageCodex::GetImage( const ImageId id ) { return m_images[id]; }

ImageId ImageCodex::LoadImageFromFile( const std::string &path, const VkFormat format, const VkImageUsageFlags usage,
                                       const bool mipmapped ) {
    // search for already loaded image
    for ( const auto &img : m_images ) {
        if ( path == img.GetName( ) && format == img.GetFormat( ) && usage == img.GetUsage( ) &&
             mipmapped == img.IsMipmapped( ) ) {
            return img.GetId( );
        }
    }

    int width, height, nr_channels;
    unsigned char *data = stbi_load( path.c_str( ), &width, &height, &nr_channels, 4 );
    if ( !data ) {
        return -1;
    }

    const VkExtent3D extent = {
            .width = static_cast<uint32_t>( width ),
            .height = static_cast<uint32_t>( height ),
            .depth = 1,
    };

    const auto id = LoadImageFromData( path, data, extent, format, usage, mipmapped );

    stbi_image_free( data );

    return id;
}

ImageId ImageCodex::LoadHdrFromFile( const std::string &path, const VkFormat format, const VkImageUsageFlags usage,
                                     const bool mipmapped ) {
    // search for already loaded image
    for ( const auto &img : m_images ) {
        if ( path == img.GetName( ) && format == img.GetFormat( ) && usage == img.GetUsage( ) &&
             mipmapped == img.IsMipmapped( ) ) {
            return img.GetId( );
        }
    }

    int width, height, nr_channels;
    float *data = stbi_loadf( path.c_str( ), &width, &height, &nr_channels, 4 );
    if ( !data ) {
        return -1;
    }

    const VkExtent3D extent = {
            .width = static_cast<uint32_t>( width ),
            .height = static_cast<uint32_t>( height ),
            .depth = 1,
    };

    const auto id = LoadImageFromData( path, data, extent, format, usage, mipmapped );

    stbi_image_free( data );

    return id;
}

ImageId ImageCodex::LoadCubemapFromFile( const std::vector<std::string> &paths, const VkFormat format,
                                         const VkImageUsageFlags usage, const bool mipmapped ) {
    assert( paths.size( ) == 6 && "Cubemap needs 6 faces!" );

    for ( const auto &img : m_images ) {
        if ( paths[0] == img.GetName( ) && format == img.GetFormat( ) && usage == img.GetUsage( ) &&
             mipmapped == img.IsMipmapped( ) ) {
            return img.GetId( );
        }
    }

    VkExtent3D extent = {
            .width = 0,
            .height = 0,
            .depth = 1,
    };

    stbi_set_flip_vertically_on_load( true );
    std::vector<unsigned char *> datas;
    for ( size_t i = 0; i < 6; i++ ) {
        auto &path = paths.at( i );

        int width, height, nr_channels;
        unsigned char *data = stbi_load( path.c_str( ), &width, &height, &nr_channels, 4 );
        if ( !data ) {
            return -1;
        }

        extent.width = width;
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

ImageId ImageCodex::LoadCubemapFromData( const std::vector<std::string> &paths,
                                         const std::vector<unsigned char *> &datas, const VkExtent3D extent,
                                         const VkFormat format, const VkImageUsageFlags usage, const bool mipmapped ) {
    const size_t total_size = image::CalculateSize( extent, format ) * datas.size( );
    const size_t face_size = image::CalculateSize( extent, format );

    unsigned char *merged_data = new unsigned char[total_size];
    size_t offset = { };

    for ( const auto &data : datas ) {
        std::memcpy( merged_data + offset, data, face_size );
        offset += face_size;
    }

    GpuImage image = GpuImage( m_gfx, paths.at( 0 ), ( void * )merged_data, extent, format, ImageType::TCubeMap, usage,
                               mipmapped );

    delete[] merged_data;

    const ImageId image_id = ( ImageId )m_images.size( );
    image.SetId( image_id );

    bindlessRegistry.AddImage( *m_gfx, image_id, image.GetBaseView( ) );
    m_images.push_back( std::move( image ) );

    return image_id;
}

ImageId ImageCodex::CreateCubemap( const std::string &name, const VkExtent3D extent, const VkFormat format,
                                   const VkImageUsageFlags usage, int mipmaps ) {
    auto image = GpuImage( m_gfx, name, extent, format, ImageType::TCubeMap, usage, mipmaps );

    const ImageId image_id = ( ImageId )m_images.size( );
    image.SetId( image_id );

    bindlessRegistry.AddImage( *m_gfx, image_id, image.GetBaseView( ) );
    m_images.push_back( std::move( image ) );
    return image_id;
}

MultiFrameImageId ImageCodex::CreateMultiFrameEmptyImage( const std::string &name, VkExtent3D extent, VkFormat format,
                                                          VkImageUsageFlags usage, bool mipmapped ) {
    std::vector<ImageId> frames;

    // upload each image to the bindless registry and to the individual id system
    for ( auto i = 0; i < TL_Swapchain::FrameOverlap; i++ ) {
        frames.push_back( CreateEmptyImage( name, extent, format, usage, mipmapped ) );
    }

    auto multi_frame = MultiFrameGpuImage( frames );
    MultiFrameImageId id = ( MultiFrameImageId )m_multiFrameImages.size( );
    multi_frame.SetId( id );
    m_multiFrameImages.push_back( std::move( multi_frame ) );

    return id;
}

ImageId ImageCodex::CreateEmptyImage( const std::string &name, const VkExtent3D extent, const VkFormat format,
                                      const VkImageUsageFlags usage, const bool mipmapped ) {
    auto image = GpuImage( m_gfx, name, extent, format, ImageType::T2D, usage, mipmapped );

    const ImageId image_id = ( ImageId )m_images.size( );
    image.SetId( image_id );

    bindlessRegistry.AddImage( *m_gfx, image_id, image.GetBaseView( ) );
    m_images.push_back( std::move( image ) );
    return image_id;
}

ImageId ImageCodex::LoadImageFromData( const std::string &name, void *data, const VkExtent3D extent,
                                       const VkFormat format, const VkImageUsageFlags usage, const bool mipmapped ) {
    auto image = GpuImage( m_gfx, name, data, extent, format, ImageType::T2D, usage, mipmapped );

    const ImageId image_id = ( ImageId )m_images.size( );
    image.SetId( image_id );

    bindlessRegistry.AddImage( *m_gfx, image_id, image.GetBaseView( ) );
    m_images.push_back( std::move( image ) );

    return image_id;
}

VkDescriptorSetLayout ImageCodex::GetBindlessLayout( ) const { return bindlessRegistry.layout; }

VkDescriptorSet ImageCodex::GetBindlessSet( ) const { return bindlessRegistry.set; }

void ImageCodex::DrawDebug( ) const {
    ImGui::Columns( 10 );
    {
        for ( u64 i = 1; i < m_images.size( ); i++ ) {
            auto &image = m_images.at( i );
            f32 column_width = ImGui::GetColumnWidth( );
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

void ImageCodex::InitDefaultImages( ) {
    // 3 default textures, white, grey, black. 1 pixel each
    uint32_t white = glm::packUnorm4x8( glm::vec4( 1, 1, 1, 1 ) );
    white = LoadImageFromData( "debug_white_img", ( void * )&white, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
                               VK_IMAGE_USAGE_SAMPLED_BIT, false );

    uint32_t grey = glm::packUnorm4x8( glm::vec4( 0.66f, 0.66f, 0.66f, 1 ) );
    grey = LoadImageFromData( "debug_grey_img", ( void * )&grey, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
                              VK_IMAGE_USAGE_SAMPLED_BIT, false );

    uint32_t black = glm::packUnorm4x8( glm::vec4( 0, 0, 0, 0 ) );
    black = LoadImageFromData( "debug_black_img", ( void * )&white, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
                               VK_IMAGE_USAGE_SAMPLED_BIT, false );

    // checkerboard image
    const uint32_t magenta = glm::packUnorm4x8( glm::vec4( 1, 0, 1, 1 ) );
    std::array<uint32_t, 16 * 16> pixels; // for 16x16 checkerboard texture
    for ( int x = 0; x < 16; x++ ) {
        for ( int y = 0; y < 16; y++ ) {
            pixels[y * 16 + x] = ( ( x % 2 ) ^ ( y % 2 ) ) ? magenta : black;
        }
    }
    m_checkboard = LoadImageFromData( "debug_checkboard_img", ( void * )&white, VkExtent3D{ 16, 16, 1 },
                                      VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, false );
}
