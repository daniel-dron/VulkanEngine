#pragma once

#include <vk_types.h>
#include <math/transform.h>

struct Scene {
	struct MeshAsset {
		std::vector<MeshID> primitives;
		std::vector<size_t> materials;
	};

	struct Node {
		int mesh_index = -1;
		std::string name;

		//Transform3D transform_3d;
		Transform transform;

		std::weak_ptr<Node> parent;
		std::vector<std::shared_ptr<Node>> children;

		void propagateMatrix( );
	};

	std::vector<MaterialID> materials;
	std::vector<MeshAsset> meshes;
	std::vector<std::shared_ptr<Node>> top_nodes;

	std::string name;
};