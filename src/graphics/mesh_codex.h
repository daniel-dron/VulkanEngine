#pragma once

#include <vk_types.h>

class GfxDevice;

struct GpuMesh {
	GpuBuffer index_buffer;
	GpuBuffer vertex_buffer;

	uint32_t index_count;
	VkDeviceAddress vertex_buffer_address;
};

struct Mesh {
	struct Vertex {
		vec3 position;
		float uv_x;
		vec3 normal;
		float uv_y;
		vec4 tangent;
	};

	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
};

class MeshCodex {
public:
	void cleanup( GfxDevice& gfx );

	MeshID addMesh( GfxDevice& gfx, const Mesh& mesh );
	const GpuMesh& getMesh( MeshID id ) const;
private:

	GpuMesh uploadMesh( GfxDevice& gfx, const Mesh& mesh );

	std::vector<GpuMesh> meshes;
};