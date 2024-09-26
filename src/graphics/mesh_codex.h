#pragma once

#include <vk_types.h>

class GfxDevice;

struct AABB {
	vec3 min;
	vec3 max;
};

struct GpuMesh {
	GpuBuffer index_buffer;
	GpuBuffer vertex_buffer;

	AABB aabb;

	uint32_t index_count;
	VkDeviceAddress vertex_buffer_address;
};

struct Mesh {
	struct Vertex {
		vec3 position;
		float uv_x;
		vec3 normal;
		float uv_y;
		vec3 tangent;
		float pad;
		vec3 bitangent;
		float pad2;
	};

	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;

	AABB aabb;
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