#include "mesh_codex.h"

#include <graphics/gfx_device.h>
#include <format>

void MeshCodex::cleanup( GfxDevice& gfx ) {
	for ( const auto& mesh : meshes ) {
		gfx.free( mesh.index_buffer );
		gfx.free( mesh.vertex_buffer );
	}
}

MeshID MeshCodex::addMesh( GfxDevice& gfx, const Mesh& mesh ) {
	auto gpu_mesh = uploadMesh( gfx, mesh );

	auto id = meshes.size( );
	meshes.push_back( gpu_mesh );

	return id;
}

const GpuMesh& MeshCodex::getMesh( MeshID id ) const {
	return meshes.at( id );
}

GpuMesh MeshCodex::uploadMesh( GfxDevice& gfx, const Mesh& mesh ) {
	GpuMesh gpu_mesh{};

	const size_t vertex_buffer_size = mesh.vertices.size( ) * sizeof( Mesh::Vertex );
	const size_t index_buffer_size = mesh.indices.size( ) * sizeof( uint32_t );

	std::string name_vertex = std::format( "{}.(vtx)", meshes.size( ) );
	std::string name_index = std::format( "{}.(idx)", meshes.size( ) );

	auto vertex_buffer = gfx.allocate( vertex_buffer_size,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VMA_MEMORY_USAGE_GPU_ONLY, name_vertex.c_str( ) );
	auto index_buffer = gfx.allocate( index_buffer_size,
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VMA_MEMORY_USAGE_GPU_ONLY, name_index.c_str( ) );

	// ----------
	// Staging
	GpuBuffer staging = gfx.allocate( vertex_buffer_size + index_buffer_size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, "Staging MeshCodex" );

	void* data = nullptr;
	vmaMapMemory( gfx.allocator, staging.allocation, &data );
	memcpy( data, mesh.vertices.data( ), vertex_buffer_size );
	memcpy( static_cast<char*>(data) + vertex_buffer_size, mesh.indices.data( ), index_buffer_size );
	vmaUnmapMemory( gfx.allocator, staging.allocation );

	gfx.execute( [&]( const VkCommandBuffer cmd ) {

		VkBufferCopy vertex_copy = {
			.srcOffset = 0,
			.dstOffset = 0,
			.size = vertex_buffer_size,
		};
		vkCmdCopyBuffer( cmd, staging.buffer, vertex_buffer.buffer, 1, &vertex_copy );

		VkBufferCopy index_copy = {
			.srcOffset = vertex_buffer_size,
			.dstOffset = 0,
			.size = index_buffer_size
		};
		vkCmdCopyBuffer( cmd, staging.buffer, index_buffer.buffer, 1, &index_copy );

	} );

	gfx.free( staging );

	gpu_mesh.index_buffer = index_buffer;
	gpu_mesh.vertex_buffer = vertex_buffer;
	gpu_mesh.index_count = mesh.indices.size( );

	VkBufferDeviceAddressInfo address_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
		.buffer = vertex_buffer.buffer
	};
	gpu_mesh.vertex_buffer_address = vkGetBufferDeviceAddress( gfx.device, &address_info );

	return gpu_mesh;
}
