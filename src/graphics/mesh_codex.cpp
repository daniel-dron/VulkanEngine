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

#include "mesh_codex.h"

#include <format>
#include <graphics/gfx_device.h>

void MeshCodex::Cleanup( GfxDevice &gfx ) const {
    for ( const auto &mesh : m_meshes ) {
        gfx.Free( mesh.indexBuffer );
        gfx.Free( mesh.vertexBuffer );
    }
}

MeshId MeshCodex::AddMesh( GfxDevice &gfx, const Mesh &mesh ) {
    const auto gpu_mesh = UploadMesh( gfx, mesh );

    const auto id = m_meshes.size( );
    m_meshes.push_back( gpu_mesh );

    return id;
}

const GpuMesh &MeshCodex::GetMesh( MeshId id ) const {
    return m_meshes.at( id );
}

GpuMesh MeshCodex::UploadMesh( GfxDevice &gfx, const Mesh &mesh ) const {
    GpuMesh gpu_mesh{ };
    gpu_mesh.aabb = mesh.aabb;

    const size_t vertex_buffer_size = mesh.vertices.size( ) * sizeof( Mesh::Vertex );
    const size_t index_buffer_size = mesh.indices.size( ) * sizeof( uint32_t );

    std::string name_vertex = std::format( "{}.(vtx)", m_meshes.size( ) );
    std::string name_index = std::format( "{}.(idx)", m_meshes.size( ) );

    auto vertex_buffer = gfx.Allocate( vertex_buffer_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                       VMA_MEMORY_USAGE_GPU_ONLY, name_vertex.c_str( ) );
    auto index_buffer = gfx.Allocate( index_buffer_size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                      VMA_MEMORY_USAGE_GPU_ONLY, name_index.c_str( ) );

    // Staging
    GpuBuffer staging = gfx.Allocate( vertex_buffer_size + index_buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, "Staging MeshCodex" );

    void *data = nullptr;
    vmaMapMemory( gfx.allocator, staging.allocation, &data );
    memcpy( data, mesh.vertices.data( ), vertex_buffer_size );
    memcpy( static_cast<char *>( data ) + vertex_buffer_size, mesh.indices.data( ), index_buffer_size );
    vmaUnmapMemory( gfx.allocator, staging.allocation );

    gfx.Execute( [&]( const VkCommandBuffer cmd ) {
        const VkBufferCopy vertex_copy = {
                .srcOffset = 0,
                .dstOffset = 0,
                .size = vertex_buffer_size,
        };
        vkCmdCopyBuffer( cmd, staging.buffer, vertex_buffer.buffer, 1, &vertex_copy );

        const VkBufferCopy index_copy = {
                .srcOffset = vertex_buffer_size,
                .dstOffset = 0,
                .size = index_buffer_size };
        vkCmdCopyBuffer( cmd, staging.buffer, index_buffer.buffer, 1, &index_copy );
    } );

    gfx.Free( staging );

    gpu_mesh.indexBuffer = index_buffer;
    gpu_mesh.vertexBuffer = vertex_buffer;
    gpu_mesh.indexCount = mesh.indices.size( );

    VkBufferDeviceAddressInfo address_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            .buffer = vertex_buffer.buffer };
    gpu_mesh.vertexBufferAddress = vkGetBufferDeviceAddress( gfx.device, &address_info );

    return gpu_mesh;
}
