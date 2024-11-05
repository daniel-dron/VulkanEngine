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

#include <memory>
#include <graphics/utils/vk_types.h>

#include <graphics/bindless.h>

class TL_VkContext;

namespace TL::renderer {

    enum ImageType {
        TUnknown,
        T2D,
        TCubeMap,
    };

    class RImage {
    public:
        using Ptr = std::shared_ptr<RImage>;

         RImage( ) = default;
         RImage( TL_VkContext* gfx, const std::string& name, void* data, VkExtent3D extent, VkFormat format,
                 ImageType imageType = T2D, VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT,
                 bool generateMipmaps = false );
         RImage( TL_VkContext* gfx, const std::string& name, VkExtent3D extent, VkFormat format, ImageType imageType = T2D,
                 VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT, bool generateMipmaps = false );
        ~RImage( );

        // delete copy constructors
                RImage( const RImage& )    = delete;
        RImage& operator=( const RImage& ) = delete;

        // allow move constructors
        RImage( RImage&& other ) noexcept :
            m_gfx( std::exchange( other.m_gfx, nullptr ) ), m_id( std::exchange( other.m_id, -1 ) ),
            m_type( std::exchange( other.m_type, TUnknown ) ), m_image( std::exchange( other.m_image, VK_NULL_HANDLE ) ),
            m_view( std::exchange( other.m_view, VK_NULL_HANDLE ) ), m_extent( std::exchange( other.m_extent, { } ) ),
            m_format( std::exchange( other.m_format, { } ) ), m_usage( std::exchange( other.m_usage, { } ) ),
            m_allocation( std::exchange( other.m_allocation, { } ) ),
            m_mipmapped( std::exchange( other.m_mipmapped, false ) ), m_mipViews( std::move( other.m_mipViews ) ),
            m_name( std::move( other.m_name ) ) {}
        RImage& operator=( RImage&& other ) noexcept {
            if ( this != &other ) {
                m_gfx        = std::exchange( other.m_gfx, nullptr );
                m_id         = std::exchange( other.m_id, -1 );
                m_type       = std::exchange( other.m_type, TUnknown );
                m_image      = std::exchange( other.m_image, VK_NULL_HANDLE );
                m_view       = std::exchange( other.m_view, VK_NULL_HANDLE );
                m_extent     = std::exchange( other.m_extent, { } );
                m_format     = std::exchange( other.m_format, { } );
                m_usage      = std::exchange( other.m_usage, { } );
                m_allocation = std::exchange( other.m_allocation, { } );
                m_mipmapped  = std::exchange( other.m_mipmapped, false );
                m_mipViews   = std::move( other.m_mipViews );
                m_name       = std::move( other.m_name );
            }
            return *this;
        }

        void TransitionLayout( VkCommandBuffer cmd, VkImageLayout currentLayout, VkImageLayout newLayout,
                               bool depth = false ) const;
        void GenerateMipmaps( VkCommandBuffer cmd ) const;

        void Resize( VkExtent3D size );

        ImageId                         GetId( ) const { return m_id; }
        void                            SetId( const ImageId new_id ) { m_id = new_id; }
        ImageType                       GetType( ) const { return m_type; }
        VkImage                         GetImage( ) const { return m_image; }
        VkImageView                     GetBaseView( ) const { return m_view; }
        VkExtent3D                      GetExtent( ) const { return m_extent; }
        VkFormat                        GetFormat( ) const { return m_format; }
        VkImageUsageFlags               GetUsage( ) const { return m_usage; }
        VmaAllocation                   GetAllocation( ) const { return m_allocation; }
        bool                            IsMipmapped( ) const { return m_mipmapped; }
        VkImageView                     GetMipView( const size_t mipLevel ) const { return m_mipViews.at( mipLevel ); }
        const std::vector<VkImageView>& GetMipViews( ) { return m_mipViews; }
        const std::string&              GetName( ) const { return m_name; }

    private:
        void SetDebugName( const std::string& name );

        void ActuallyCreateEmptyImage2D( );
        void ActuallyCreateImage2DFromData( const void* data );
        void ActuallyCreateCubemapFromData( const void* data );
        void ActuallyCreateEmptyCubemap( );

        TL_VkContext* m_gfx;

        ImageId m_id = -1;

        ImageType m_type = TUnknown;

        VkImage           m_image  = VK_NULL_HANDLE;
        VkImageView       m_view   = VK_NULL_HANDLE;
        VkExtent3D        m_extent = { };
        VkFormat          m_format = { };
        VkImageUsageFlags m_usage  = { };

        VmaAllocation m_allocation = { };

        bool                     m_mipmapped = false;
        std::vector<VkImageView> m_mipViews;

        std::string m_name;
    };

    class ImageCodex {
    public:
        static constexpr ImageId InvalidImageId = std::numeric_limits<uint32_t>::max( ) - 1;

        void Init( TL_VkContext* gfx );
        void Cleanup( );

        const std::vector<RImage>& GetImages( );
        RImage&                    GetImage( ImageId );

        ImageId LoadImageFromFile( const std::string& path, VkFormat format, VkImageUsageFlags usage, bool mipmapped );
        ImageId LoadHdrFromFile( const std::string& path, VkFormat format, VkImageUsageFlags usage, bool mipmapped );
        ImageId LoadImageFromData( const std::string& name, void* data, VkExtent3D extent, VkFormat format, VkImageUsageFlags usage, bool mipmapped );
        ImageId LoadCubemapFromFile( const std::vector<std::string>& paths, VkFormat format, VkImageUsageFlags usage, bool mipmapped );
        ImageId LoadCubemapFromData( const std::vector<std::string>& paths, const std::vector<unsigned char*>& datas, VkExtent3D extent, VkFormat format, VkImageUsageFlags usage, bool mipmapped );
        ImageId CreateEmptyImage( const std::string& name, VkExtent3D extent, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false );
        ImageId CreateCubemap( const std::string& name, VkExtent3D extent, VkFormat format, VkImageUsageFlags usage, int mipmaps = 0 );

        VkDescriptorSetLayout GetBindlessLayout( ) const;
        VkDescriptorSet       GetBindlessSet( ) const;

        ImageId GetWhiteImageId( ) const { return m_white; }
        ImageId GetBlackImageId( ) const { return m_black; }
        ImageId GetGreyImageId( ) const { return m_grey; }
        ImageId GetChekboardImageId( ) const { return m_checkboard; }

        void UnloadIamge( ImageId );

        BindlessRegistry bindlessRegistry;

        void DrawDebug( ) const;

    private:
        void InitDefaultImages( );

        ImageId GetAvailableId( );

        ImageId m_white      = InvalidImageId;
        ImageId m_black      = InvalidImageId;
        ImageId m_grey       = InvalidImageId;
        ImageId m_checkboard = InvalidImageId;

        std::vector<RImage>  m_images;
        std::vector<ImageId> m_freeIds;
        TL_VkContext*        m_gfx = nullptr;
    };

    namespace image {
        size_t CalculateSize( VkExtent3D extent, VkFormat format );

        void Allocate2D( VmaAllocator allocator, VkFormat format, VkExtent3D extent, VkImageUsageFlags usage, uint32_t mipLevels, VkImage* image, VmaAllocation* allocation );
        void AllocateCubemap( VmaAllocator allocator, VkFormat format, VkExtent3D extent, VkImageUsageFlags usage, uint32_t mipLevels, VkImage* image, VmaAllocation* allocation );

        VkImageView CreateView2D( VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevel = 0 );
        VkImageView CreateViewCubemap( VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevel = 0 );

        void TransitionLayout( VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout, bool depth = false );

        // expects image to be in layout VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        // will leave image in layout VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
        void GenerateMipmaps( VkCommandBuffer cmd, VkImage image, VkExtent2D imageSize );

        // expects image to be in layout VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        void CopyFromBuffer( VkCommandBuffer cmd, VkBuffer buffer, VkImage image, VkExtent3D extent, VkImageAspectFlags aspect, VkDeviceSize offset = 0, uint32_t face = 0 );

        void Blit( VkCommandBuffer cmd, VkImage srcImage, VkExtent2D srcExtent, VkImage dstImage, VkExtent2D dstExtent, VkFilter filter = VK_FILTER_LINEAR );
    } // namespace image

} // namespace TL::renderer
