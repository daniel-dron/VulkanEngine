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

namespace TL {

    enum class BufferType { TIndex, TVertex, TConstant, TMax };

    class Buffer {
    public:
        Buffer( ) = default;
        Buffer( BufferType type, u64 size, u32 count, const void *data, const std::string &name );
        ~Buffer( );

        // If size is 0, it will default to the original size passed through the constructor
        void Upload( const void *data, u32 size = 0 );

        // Call this at the end of the frame to prepare it for next frame usage
        void AdvanceFrame( );

        VkBuffer GetVkResource( ) const { return m_buffer; }
        VkDeviceAddress GetDeviceAddress( ) const {
            assert( m_type == BufferType::TConstant ||
                    m_type == BufferType::TVertex && "Device Address only allowed for constant and vertex buffers" );
            return m_deviceAddress;
        }

    private:
        VkBuffer m_buffer = VK_NULL_HANDLE;

        BufferType m_type = BufferType::TMax;
        u64 m_size = 0;
        u32 m_count = 0;

        // Used to offset into the right buffer position for the current frame
        u64 m_offset = 0;

        VmaAllocation m_allocation = { };
        VmaAllocationInfo m_allocationInfo = { };

        VkDeviceAddress m_deviceAddress = 0;
        u64 m_gpuData = 0;

        std::string m_resourceName;
    };

} // namespace TL