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

#include "tl_buffer.h"

namespace TL {

    Buffer::Buffer( BufferType type, u64 size, u32 count, const void *data, const std::string &name ) {
        assert( type != BufferType::TMax && "Invalid buffer type" );
        assert( size != 0 && "Invalid buffer size" );
        assert( count != 0 && "Invalid buffer count" );

        assert( data == nullptr && "TODO" );

        m_type = type;

        // Round up to the next alignment border
        m_size         = size;
        m_count        = count;
        m_resourceName = name;

        VkBufferUsageFlags       usage     = 0;
        VmaAllocationCreateFlags vma_flags = 0;
        VmaMemoryUsage           vma_usage = { };

        if ( type == BufferType::TConstant ) {
            const auto alignment = vkctx->deviceProperties.properties.limits.minUniformBufferOffsetAlignment;
            m_size               = ( size + alignment - 1 ) & ~( alignment - 1 );
            usage                = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
            vma_flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
            vma_usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        }
        else if ( type == BufferType::TIndex ) {
            assert( false );
        }
        else if ( type == BufferType::TVertex ) {
            assert( false );
        }
        else if ( type == BufferType::TStorage ) {
            usage     = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
            vma_flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
            vma_usage = VMA_MEMORY_USAGE_AUTO;
        }
        else {
            assert( false );
        }

        const VkBufferCreateInfo      info     = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                                   .pNext = nullptr,
                                                   .size  = m_size * m_count,
                                                   .usage = usage };
        const VmaAllocationCreateInfo vma_info = { .flags = vma_flags, .usage = vma_usage };
        VKCALL( vmaCreateBuffer( vkctx->allocator, &info, &vma_info, &m_buffer, &m_allocation, &m_allocationInfo ) );

        // If its a constant buffer then we will get the device address and also map it
        if ( m_type == BufferType::TConstant || m_type == BufferType::TStorage ) {
            const VkBufferDeviceAddressInfo address_info = {
                    .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .pNext = nullptr, .buffer = m_buffer };
            m_deviceAddress = vkGetBufferDeviceAddress( vkctx->device, &address_info );
            assert( m_deviceAddress && "Could not map gpu constant buffer" );

            vmaMapMemory( vkctx->allocator, m_allocation, reinterpret_cast<void **>( &m_gpuData ) );
        }
    }

    Buffer::~Buffer( ) {
        if ( m_gpuData ) {
            vmaUnmapMemory( vkctx->allocator, m_allocation );
        }

        vmaDestroyBuffer( vkctx->allocator, m_buffer, m_allocation );

        m_buffer         = VK_NULL_HANDLE;
        m_type           = BufferType::TMax;
        m_size           = 0;
        m_count          = 0;
        m_allocation     = { };
        m_allocationInfo = { };
        m_deviceAddress  = 0;
    }

    void Buffer::Upload( const void *data, const u32 size ) const { Upload( data, 0, size ); }

    void Buffer::Upload( const void *data, const u64 offset, const u32 size ) const {
        const auto actual_size = ( size == 0 ) ? m_size : size;
        memcpy( reinterpret_cast<void *>( m_gpuData + m_offset + offset ), data, actual_size );
    }

    void Buffer::AdvanceFrame( ) {
        m_offset += m_size;
        m_offset = m_offset % ( m_size * m_count );
    }


} // namespace TL