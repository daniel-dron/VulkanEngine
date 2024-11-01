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

#include <graphics/bindless.h>
#include <graphics/gpu_image.h>
#include <vk_types.h>

class TL_VkContext;

class ImageCodex {
public:
    static constexpr ImageId InvalidImageId = std::numeric_limits<uint32_t>::max( ) - 1;

    void Init( TL_VkContext *gfx );
    void Cleanup( );

    const std::vector<GpuImage> &GetImages( );
    GpuImage                    &GetImage( ImageId );
    ImageId LoadImageFromFile( const std::string &path, VkFormat format, VkImageUsageFlags usage, bool mipmapped );
    ImageId LoadHdrFromFile( const std::string &path, VkFormat format, VkImageUsageFlags usage, bool mipmapped );
    ImageId LoadImageFromData( const std::string &name, void *data, VkExtent3D extent, VkFormat format,
                               VkImageUsageFlags usage, bool mipmapped );
    ImageId LoadCubemapFromFile( const std::vector<std::string> &paths, VkFormat format, VkImageUsageFlags usage,
                                 bool mipmapped );
    ImageId LoadCubemapFromData( const std::vector<std::string> &paths, const std::vector<unsigned char *> &datas,
                                 VkExtent3D extent, VkFormat format, VkImageUsageFlags usage, bool mipmapped );

    ImageId CreateEmptyImage( const std::string &name, VkExtent3D extent, VkFormat format, VkImageUsageFlags usage,
                              bool mipmapped = false );
    ImageId CreateCubemap( const std::string &name, VkExtent3D extent, VkFormat format, VkImageUsageFlags usage,
                           int mipmaps = 0 );

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

    std::vector<GpuImage> m_images;
    std::vector<ImageId>  m_freeIds;
    TL_VkContext         *m_gfx = nullptr;
};
