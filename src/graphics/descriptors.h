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

#include <span>

class GfxDevice;

class DescriptorLayoutBuilder {
public:
    void AddBinding( uint32_t binding, VkDescriptorType type );
    void Clear( );

    VkDescriptorSetLayout Build( VkDevice device, VkShaderStageFlags shaderStages, VkDescriptorSetLayoutCreateFlags flags = 0 );

private:
    std::vector<VkDescriptorSetLayoutBinding> m_bindings;
};

class DescriptorAllocator {
public:
    struct PoolSizeRatio {
        VkDescriptorType type;
        float ratio;
    };

    VkDescriptorPool pool;

    void InitPool( VkDevice device, uint32_t maxSets, std::span<PoolSizeRatio> poolRatios );
    void ClearDescriptors( VkDevice device );
    void DestroyPool( VkDevice device );

    VkDescriptorSet Allocate( VkDevice device, VkDescriptorSetLayout layout );
};

class DescriptorAllocatorGrowable {
public:
    struct PoolSizeRatio {
        VkDescriptorType type;
        float ratio;
    };

    void Init( VkDevice device, uint32_t maxSets, std::span<PoolSizeRatio> poolRatios );
    void ClearPools( VkDevice device );
    void DestroyPools( VkDevice device );

    VkDescriptorSet Allocate( VkDevice device, VkDescriptorSetLayout layout, const void *pNext = nullptr );

private:
    VkDescriptorPool GetPool( VkDevice device );
    VkDescriptorPool CreatePool( VkDevice device, uint32_t setCount, std::span<PoolSizeRatio> poolRatios );

    std::vector<PoolSizeRatio> m_ratios;
    std::vector<VkDescriptorPool> m_fullPools;
    std::vector<VkDescriptorPool> m_readyPools;
    uint32_t m_setsPerPool = 0;
};

class DescriptorWriter {
public:
    std::deque<VkDescriptorImageInfo> imageInfos;
    std::deque<VkDescriptorBufferInfo> bufferInfos;
    std::vector<VkWriteDescriptorSet> writes;

    void WriteImage( int binding, VkImageView image, VkSampler sampler, VkImageLayout layout, VkDescriptorType type );
    void WriteBuffer( int binding, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType type );

    void Clear( );
    void UpdateSet( VkDevice device, VkDescriptorSet set );
};

namespace descriptor {
    VkDescriptorSetLayoutBinding CreateLayoutBinding( uint32_t binding, VkDescriptorType type );
    VkDescriptorSetLayout CreateDescriptorSetLayout( VkDevice device, VkDescriptorSetLayoutBinding *bindings, uint32_t count, VkDescriptorSetLayoutCreateFlags flags );
} // namespace descriptor
