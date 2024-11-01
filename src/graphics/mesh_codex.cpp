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
#include <graphics/tl_vkcontext.h>

void MeshCodex::Cleanup( TL_VkContext &gfx ) {
    for ( auto &mesh : m_meshes ) {
        for ( auto &buffer : mesh.indexBuffer ) {
            buffer.reset( );
        }

        mesh.vertexBuffer.reset( );
    }
}

MeshId MeshCodex::AddMesh( TL_VkContext &gfx, const Mesh &mesh ) {
    const auto gpu_mesh = UploadMesh( gfx, mesh );

    const auto id = ( MeshId )m_meshes.size( );
    m_meshes.push_back( gpu_mesh );

    return id;
}

const GpuMesh &MeshCodex::GetMesh( MeshId id ) const { return m_meshes.at( id ); }

GpuMesh MeshCodex::UploadMesh( TL_VkContext &gfx, const Mesh &mesh ) const {
    GpuMesh gpu_mesh{ };
    gpu_mesh.aabb = mesh.aabb;

    const size_t vertex_buffer_size = mesh.vertices.size( ) * sizeof( Mesh::Vertex );
    const size_t index_buffer_size  = mesh.indices[0].size( ) * sizeof( uint32_t );

    std::string name_vertex = std::format( "{}.(vtx)", m_meshes.size( ) );
    std::string name_index  = std::format( "{}.(idx)", m_meshes.size( ) );


    // Staging
    {
        auto vertex_buffer = std::make_shared<TL::Buffer>( TL::BufferType::TVertex, vertex_buffer_size, 1, nullptr,
                                                           name_vertex.c_str( ) );
        gpu_mesh.vertexBufferAddress = vertex_buffer->GetDeviceAddress( );

        auto index_buffer = std::make_shared<TL::Buffer>( TL::BufferType::TIndex, index_buffer_size, 1, nullptr,
                                                          name_index.c_str( ) );
        auto staging =
                TL::Buffer( TL::BufferType::TStaging, vertex_buffer_size + index_buffer_size, 1, nullptr, "Staging" );

        staging.Upload( mesh.vertices.data( ), vertex_buffer_size );
        staging.Upload( mesh.indices[0].data( ), vertex_buffer_size, index_buffer_size );

        gfx.Execute( [&]( const VkCommandBuffer cmd ) {
            const VkBufferCopy vertex_copy = {
                    .srcOffset = 0,
                    .dstOffset = 0,
                    .size      = vertex_buffer_size,
            };
            vkCmdCopyBuffer( cmd, staging.GetVkResource( ), vertex_buffer->GetVkResource( ), 1, &vertex_copy );

            const VkBufferCopy index_copy = {
                    .srcOffset = vertex_buffer_size, .dstOffset = 0, .size = index_buffer_size };
            vkCmdCopyBuffer( cmd, staging.GetVkResource( ), index_buffer->GetVkResource( ), 1, &index_copy );
        } );

        gpu_mesh.indexBuffer.push_back( index_buffer );
        gpu_mesh.vertexBuffer = vertex_buffer;
        gpu_mesh.indexCount.push_back( ( u32 )mesh.indices[0].size( ) );
    }

    // Time for LODs
    for ( int i = 1; i < mesh.indices.size( ); i++ ) {
        if ( mesh.indices[i].empty( ) ) {
            continue;
        }
        auto index_buffer_lod = std::make_shared<TL::Buffer>( TL::BufferType::TIndex,
                                                              mesh.indices[i].size( ) * sizeof( u32 ), 1, nullptr, "" );

        auto staging =
                TL::Buffer( TL::BufferType::TStaging, mesh.indices[i].size( ) * sizeof( u32 ), 1, nullptr, "Staging" );
        staging.Upload( mesh.indices[i].data( ), sizeof( u32 ) * mesh.indices[i].size( ) );

        gfx.Execute( [&]( const VkCommandBuffer cmd ) {
            const VkBufferCopy copy = {
                    .srcOffset = 0,
                    .dstOffset = 0,
                    .size      = sizeof( uint32_t ) * mesh.indices[i].size( ),
            };
            vkCmdCopyBuffer( cmd, staging.GetVkResource( ), index_buffer_lod->GetVkResource( ), 1, &copy );
        } );

        gpu_mesh.indexBuffer.push_back( index_buffer_lod );
        gpu_mesh.indexCount.push_back( ( u32 )mesh.indices[i].size( ) );
    }


    return gpu_mesh;
}
