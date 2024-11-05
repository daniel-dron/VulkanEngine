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

#include <graphics/utils/vk_types.h>

class TL_VkContext;

class BindlessRegistry {
public:
    void Init( TL_VkContext &gfx );
    void Cleanup( const TL_VkContext &gfx );

    void AddImage( const TL_VkContext &gfx, ImageId id, const VkImageView view );
    void AddSampler( TL_VkContext &gfx, uint32_t id, const VkSampler sampler );
    void AddStorageImage( TL_VkContext &gfx, ImageId id, const VkImageView view );

    static constexpr size_t MaxBindlessImages = 16000;
    static constexpr size_t MaxSamplers = 3;
    static constexpr size_t TextureBinding = 0;
    static constexpr size_t SamplersBinding = 1;
    static constexpr size_t StorageBinding = 2;

    VkDescriptorPool pool;
    VkDescriptorSetLayout layout;
    VkDescriptorSet set;

    VkSampler nearestSampler;
    VkSampler linearSampler;
    VkSampler shadowMapSampler;

private:
    void InitSamplers( TL_VkContext &gfx );
};
