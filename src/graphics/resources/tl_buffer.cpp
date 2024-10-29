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

        m_type = type;
        m_size = size;
        m_count = count;
        m_resourceName = name;

        VkBufferUsageFlags usage = 0;
        VmaAllocationCreateFlags vma_flags = 0;
        VmaMemoryUsage vma_usage = { };

        if ( type == BufferType::TConstant ) {
            usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
            vma_flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
            vma_usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        }
        else if ( type == BufferType::TIndex ) {
            assert( false );
        }
        else if ( type == BufferType::TVertex ) {
            assert( false );
        }
        else {
            assert( false );
        }

        const VkBufferCreateInfo info = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                          .pNext = nullptr,
                                          .size = m_size * m_count,
                                          .usage = usage };
        const VmaAllocationCreateInfo vma_info = { .flags = vma_flags, .usage = vma_usage };
        VKCALL( vmaCreateBuffer( TL_Engine::Get( ).renderer->vkctx.allocator, &info, &vma_info, &m_buffer,
                                 &m_allocation, &m_allocationInfo ) );

        // If its a constant buffer then we will get the device address and also map it
        if ( m_type == BufferType::TConstant ) {
            const VkBufferDeviceAddressInfo address_info = {
                    .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .pNext = nullptr, .buffer = m_buffer };
            m_deviceAddress = vkGetBufferDeviceAddress( TL_Engine::Get( ).renderer->vkctx.device, &address_info );
            assert( m_deviceAddress && "Could not map gpu constant buffer" );

            vmaMapMemory( TL_Engine::Get( ).renderer->vkctx.allocator, m_allocation,
                          reinterpret_cast<void **>( &m_gpuData ) );
        }
    }

    Buffer::~Buffer( ) {
        if ( m_gpuData ) {
            vmaUnmapMemory( TL_Engine::Get( ).renderer->vkctx.allocator, m_allocation );
        }

        vmaDestroyBuffer( TL_Engine::Get( ).renderer->vkctx.allocator, m_buffer, m_allocation );

        m_buffer = VK_NULL_HANDLE;
        m_type = BufferType::TMax;
        m_size = 0;
        m_count = 0;
        m_allocation = { };
        m_allocationInfo = { };
        m_deviceAddress = 0;
    }

    void Buffer::Upload( const void *data, const u32 size ) {
        const auto actual_size = ( size == 0 ) ? m_size : size;
        memcpy( reinterpret_cast<void *>( m_gpuData + m_offset ), data, actual_size );
    }

    void Buffer::AdvanceFrame( ) {
        m_offset += m_size;
        m_offset = m_offset % ( m_size * m_count );
    }


} // namespace TL