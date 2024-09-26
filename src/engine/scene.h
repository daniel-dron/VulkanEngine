#pragma once

#include <vk_types.h>
#include <camera/camera.h>
#include <math/transform.h>

struct Scene {
	struct MeshAsset {
		MeshID mesh;
		size_t material;
	};

	struct Node {
		std::vector<int> mesh_ids;

		std::string name;

		Transform transform;

		std::weak_ptr<Node> parent;
		std::vector<std::shared_ptr<Node>> children;
	
		void setTransform( const mat4& new_transform );
		mat4 getTransformMatrix( ) const;
	};

	std::vector<MaterialID> materials;
	std::vector<MeshAsset> meshes;
	std::vector<std::shared_ptr<Node>> top_nodes;
	std::vector<Camera> cameras;

	std::string name;
};