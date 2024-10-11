#pragma once

#include <vk_types.h>
#include <camera/camera.h>
#include <math/transform.h>

#include <memory>

struct PointLight;
struct DirectionalLight;

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

	std::shared_ptr<Node> FindNodeByName( const std::string& name ) const;

	std::vector<MaterialID> materials;
	std::vector<MeshAsset> meshes;
	std::vector<std::shared_ptr<Node>> top_nodes;
	std::vector<Camera> cameras;
	std::vector<PointLight> point_lights;
	std::vector<DirectionalLight> directional_lights;

	std::string name;
};