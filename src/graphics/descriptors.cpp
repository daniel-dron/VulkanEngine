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

#include "descriptors.h"

void DescriptorLayoutBuilder::AddBinding( uint32_t binding, VkDescriptorType type ) {
    m_bindings.push_back( descriptor::CreateLayoutBinding( binding, type ) );
}

void DescriptorLayoutBuilder::Clear( ) { m_bindings.clear( ); }

VkDescriptorSetLayout DescriptorLayoutBuilder::Build( VkDevice device, VkShaderStageFlags shaderStages, VkDescriptorSetLayoutCreateFlags flags ) {
    for ( auto &b : m_bindings ) {
        b.stageFlags |= shaderStages;
    }

    return descriptor::CreateDescriptorSetLayout( device, m_bindings.data( ), m_bindings.size( ), flags );
}

void DescriptorAllocator::InitPool( VkDevice device, uint32_t maxSets, std::span<PoolSizeRatio> poolRatios ) {
    std::vector<VkDescriptorPoolSize> pool_sizes;
    for ( const PoolSizeRatio ratio : poolRatios ) {
        pool_sizes.push_back( VkDescriptorPoolSize{ .type = ratio.type, .descriptorCount = static_cast<uint32_t>( ratio.ratio * maxSets ) } );
    }

    VkDescriptorPoolCreateInfo pool_info = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    pool_info.flags = 0;
    pool_info.maxSets = maxSets;
    pool_info.poolSizeCount = static_cast<uint32_t>( pool_sizes.size( ) );
    pool_info.pPoolSizes = pool_sizes.data( );

    vkCreateDescriptorPool( device, &pool_info, nullptr, &pool );
}

void DescriptorAllocator::ClearDescriptors( VkDevice device ) {
    vkResetDescriptorPool( device, pool, 0 );
}

void DescriptorAllocator::DestroyPool( VkDevice device ) {
    vkDestroyDescriptorPool( device, pool, nullptr );
}

VkDescriptorSet DescriptorAllocator::Allocate( VkDevice device, VkDescriptorSetLayout layout ) {
    VkDescriptorSetAllocateInfo alloc_info = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    alloc_info.pNext = nullptr;
    alloc_info.descriptorPool = pool;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &layout;

    VkDescriptorSet ds;
    VK_CHECK( vkAllocateDescriptorSets( device, &alloc_info, &ds ) );

    return ds;
}

VkDescriptorPool DescriptorAllocatorGrowable::GetPool( VkDevice device ) {
    VkDescriptorPool newPool;
    if ( m_readyPools.size( ) != 0 ) {
        newPool = m_readyPools.back( );
        m_readyPools.pop_back( );
    }
    else {
        // need to create a new pool
        newPool = CreatePool( device, m_setsPerPool, m_ratios );

        m_setsPerPool = m_setsPerPool * 1.5;
        if ( m_setsPerPool > 4092 ) {
            m_setsPerPool = 4092;
        }
    }

    return newPool;
}

VkDescriptorPool DescriptorAllocatorGrowable::CreatePool( VkDevice device, uint32_t setCount, std::span<PoolSizeRatio> poolRatios ) {
    std::vector<VkDescriptorPoolSize> pool_sizes;
    for ( PoolSizeRatio ratio : poolRatios ) {
        pool_sizes.push_back( VkDescriptorPoolSize{
                .type = ratio.type,
                .descriptorCount = static_cast<uint32_t>( ratio.ratio * setCount ) } );
    }

    VkDescriptorPoolCreateInfo pool_info = { };
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = 0;
    pool_info.maxSets = setCount;
    pool_info.poolSizeCount = static_cast<uint32_t>( pool_sizes.size( ) );
    pool_info.pPoolSizes = pool_sizes.data( );

    VkDescriptorPool new_pool;
    vkCreateDescriptorPool( device, &pool_info, nullptr, &new_pool );
    return new_pool;
}

void DescriptorAllocatorGrowable::Init( VkDevice device, uint32_t maxSets, std::span<PoolSizeRatio> poolRatios ) {
    m_ratios.clear( );

    for ( auto r : poolRatios ) {
        m_ratios.push_back( r );
    }

    const VkDescriptorPool new_pool = CreatePool( device, maxSets, poolRatios );

    m_setsPerPool = maxSets * 1.5; // grow it next allocation

    m_readyPools.push_back( new_pool );
}

void DescriptorAllocatorGrowable::ClearPools( VkDevice device ) {
    for ( auto p : m_readyPools ) {
        vkResetDescriptorPool( device, p, 0 );
    }
    for ( auto p : m_fullPools ) {
        vkResetDescriptorPool( device, p, 0 );
        m_readyPools.push_back( p );
    }
    m_fullPools.clear( );
}

void DescriptorAllocatorGrowable::DestroyPools( VkDevice device ) {
    for ( auto p : m_readyPools ) {
        vkDestroyDescriptorPool( device, p, nullptr );
    }
    m_readyPools.clear( );
    for ( auto p : m_fullPools ) {
        vkDestroyDescriptorPool( device, p, nullptr );
    }
    m_fullPools.clear( );
}

VkDescriptorSet DescriptorAllocatorGrowable::Allocate( VkDevice device, VkDescriptorSetLayout layout, const void *pNext ) {
    // get or create a pool to allocate from
    VkDescriptorPool pool_to_use = GetPool( device );

    VkDescriptorSetAllocateInfo alloc_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext = pNext,
            .descriptorPool = pool_to_use,
            .descriptorSetCount = 1,
            .pSetLayouts = &layout,
    };

    VkDescriptorSet ds;
    const VkResult result = vkAllocateDescriptorSets( device, &alloc_info, &ds );

    // allocation failed. Try again
    if ( result == VK_ERROR_OUT_OF_POOL_MEMORY ||
         result == VK_ERROR_FRAGMENTED_POOL ) {
        m_fullPools.push_back( pool_to_use );

        pool_to_use = GetPool( device );
        alloc_info.descriptorPool = pool_to_use;

        VK_CHECK( vkAllocateDescriptorSets( device, &alloc_info, &ds ) );
    }

    m_readyPools.push_back( pool_to_use );
    return ds;
}

void DescriptorWriter::WriteBuffer( int binding, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType type ) {
    const VkDescriptorBufferInfo &info = bufferInfos.emplace_back( VkDescriptorBufferInfo{ .buffer = buffer, .offset = offset, .range = size } );

    VkWriteDescriptorSet write = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };

    write.dstBinding = binding;
    write.dstSet = VK_NULL_HANDLE; // left empty for now until we need to write it
    write.descriptorCount = 1;
    write.descriptorType = type;
    write.pBufferInfo = &info;

    writes.push_back( write );
}

void DescriptorWriter::WriteImage( int binding, VkImageView image, VkSampler sampler, VkImageLayout layout, VkDescriptorType type ) {
    const VkDescriptorImageInfo &info = imageInfos.emplace_back( VkDescriptorImageInfo{ .sampler = sampler, .imageView = image, .imageLayout = layout } );

    VkWriteDescriptorSet write = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };

    write.dstBinding = binding;
    write.dstSet = VK_NULL_HANDLE;
    write.descriptorCount = 1;
    write.descriptorType = type;
    write.pImageInfo = &info;

    writes.push_back( write );
}

void DescriptorWriter::Clear( ) {
    imageInfos.clear( );
    writes.clear( );
    bufferInfos.clear( );
}

void DescriptorWriter::UpdateSet( VkDevice device, VkDescriptorSet set ) {
    for ( VkWriteDescriptorSet &write : writes ) {
        write.dstSet = set;
    }

    vkUpdateDescriptorSets( device, static_cast<uint32_t>( writes.size( ) ), writes.data( ), 0, nullptr );
}

VkDescriptorSetLayoutBinding descriptor::CreateLayoutBinding( uint32_t binding, VkDescriptorType type ) {
    return VkDescriptorSetLayoutBinding{
            .binding = binding,
            .descriptorType = type,
            .descriptorCount = 1,
    };
}

VkDescriptorSetLayout descriptor::CreateDescriptorSetLayout( VkDevice device, VkDescriptorSetLayoutBinding *bindings, uint32_t count, VkDescriptorSetLayoutCreateFlags flags ) {
    const VkDescriptorSetLayoutCreateInfo info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext = nullptr,
            .flags = flags,
            .bindingCount = count,
            .pBindings = bindings,
    };

    VkDescriptorSetLayout layout;
    VK_CHECK( vkCreateDescriptorSetLayout( device, &info, nullptr, &layout ) );

    return layout;
}
