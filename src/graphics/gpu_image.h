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

#pragma once

#include <vk_types.h>

class GfxDevice;

enum ImageType {
    TUnknown,
    T2D,
    TCubeMap,
};

class GpuImage {
public:
    using Ptr = std::shared_ptr<GpuImage>;

    GpuImage( ) = delete;
    GpuImage( GfxDevice *gfx, const std::string &name, void *data, VkExtent3D extent, VkFormat format,
              ImageType imageType = T2D, VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT,
              bool generateMipmaps = false );
    GpuImage( GfxDevice *gfx, const std::string &name, VkExtent3D extent, VkFormat format, ImageType imageType = T2D,
              VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT, bool generateMipmaps = false );
    ~GpuImage( );

    // delete copy constructors
    GpuImage( const GpuImage & ) = delete;
    GpuImage &operator=( const GpuImage & ) = delete;

    // allow move constructors
    GpuImage( GpuImage &&other ) noexcept :
        m_gfx( std::exchange( other.m_gfx, nullptr ) ), m_id( std::exchange( other.m_id, -1 ) ),
        m_type( std::exchange( other.m_type, TUnknown ) ), m_image( std::exchange( other.m_image, VK_NULL_HANDLE ) ),
        m_view( std::exchange( other.m_view, VK_NULL_HANDLE ) ), m_extent( std::exchange( other.m_extent, { } ) ),
        m_format( std::exchange( other.m_format, { } ) ), m_usage( std::exchange( other.m_usage, { } ) ),
        m_allocation( std::exchange( other.m_allocation, { } ) ),
        m_mipmapped( std::exchange( other.m_mipmapped, false ) ), m_mipViews( std::move( other.m_mipViews ) ),
        m_name( std::move( other.m_name ) ) {}
    GpuImage &operator=( GpuImage &&other ) noexcept {
        if ( this != &other ) {
            m_gfx = std::exchange( other.m_gfx, nullptr );
            m_id = std::exchange( other.m_id, -1 );
            m_type = std::exchange( other.m_type, TUnknown );
            m_image = std::exchange( other.m_image, VK_NULL_HANDLE );
            m_view = std::exchange( other.m_view, VK_NULL_HANDLE );
            m_extent = std::exchange( other.m_extent, { } );
            m_format = std::exchange( other.m_format, { } );
            m_usage = std::exchange( other.m_usage, { } );
            m_allocation = std::exchange( other.m_allocation, { } );
            m_mipmapped = std::exchange( other.m_mipmapped, false );
            m_mipViews = std::move( other.m_mipViews );
            m_name = std::move( other.m_name );
        }
        return *this;
    }

    void TransitionLayout( VkCommandBuffer cmd, VkImageLayout currentLayout, VkImageLayout newLayout,
                           bool depth = false ) const;
    void GenerateMipmaps( VkCommandBuffer cmd ) const;

    ImageId GetId( ) const { return m_id; }
    void SetId( const ImageId new_id ) { m_id = new_id; }
    ImageType GetType( ) const { return m_type; }
    VkImage GetImage( ) const { return m_image; }
    VkImageView GetBaseView( ) const { return m_view; }
    VkExtent3D GetExtent( ) const { return m_extent; }
    VkFormat GetFormat( ) const { return m_format; }
    VkImageUsageFlags GetUsage( ) const { return m_usage; }
    VmaAllocation GetAllocation( ) const { return m_allocation; }
    bool IsMipmapped( ) const { return m_mipmapped; }
    VkImageView GetMipView( const size_t mipLevel ) const { return m_mipViews.at( mipLevel ); }
    const std::vector<VkImageView> &GetMipViews( ) { return m_mipViews; }
    const std::string &GetName( ) const { return m_name; }

private:
    void SetDebugName( const std::string &name );

    void ActuallyCreateEmptyImage2D( );
    void ActuallyCreateImage2DFromData( const void *data );
    void ActuallyCreateCubemapFromData( const void *data );
    void ActuallyCreateEmptyCubemap( );

private:
    GfxDevice *m_gfx;

    ImageId m_id = -1;

    ImageType m_type = TUnknown;

    VkImage m_image = VK_NULL_HANDLE;
    VkImageView m_view = VK_NULL_HANDLE;
    VkExtent3D m_extent = { };
    VkFormat m_format = { };
    VkImageUsageFlags m_usage = { };

    VmaAllocation m_allocation = { };

    bool m_mipmapped = false;
    std::vector<VkImageView> m_mipViews;

    std::string m_name;
};

namespace image {
    size_t CalculateSize( VkExtent3D extent, VkFormat format );

    void Allocate2D( VmaAllocator allocator, VkFormat format, VkExtent3D extent, VkImageUsageFlags usage,
                     uint32_t mipLevels, VkImage *image, VmaAllocation *allocation );
    void AllocateCubemap( VmaAllocator allocator, VkFormat format, VkExtent3D extent, VkImageUsageFlags usage,
                          uint32_t mipLevels, VkImage *image, VmaAllocation *allocation );

    VkImageView CreateView2D( VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags,
                              uint32_t mipLevel = 0 );
    VkImageView CreateViewCubemap( VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags,
                                   uint32_t mipLevel = 0 );

    void TransitionLayout( VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout,
                           bool depth = false );

    // expects image to be in layout VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    // will leave image in layout VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
    void GenerateMipmaps( VkCommandBuffer cmd, VkImage image, VkExtent2D imageSize );

    // expects image to be in layout VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    void CopyFromBuffer( VkCommandBuffer cmd, VkBuffer buffer, VkImage image, VkExtent3D extent,
                         VkImageAspectFlags aspect, VkDeviceSize offset = 0, uint32_t face = 0 );

    void Blit( VkCommandBuffer cmd, VkImage srcImage, VkExtent2D srcExtent, VkImage dstImage, VkExtent2D dstExtent,
               VkFilter filter = VK_FILTER_LINEAR );
} // namespace image
