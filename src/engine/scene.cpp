#include "scene.h"
#include <imgui.h>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/euler_angles.hpp>

void Scene::Node::setTransform( const mat4& new_transform ) {
	glm::vec3 skew{};
	glm::vec4 perspective{};
	glm::quat rotation{};

	glm::decompose( new_transform, transform.scale, rotation, transform.position, skew, perspective );

	glm::extractEulerAngleXYZ( glm::mat4_cast( rotation ), transform.euler.x, transform.euler.y, transform.euler.z );

	transform.model = new_transform;
}

mat4 Scene::Node::getTransformMatrix( ) const {
	mat4 localTransform = transform.asMatrix( );

	if ( auto parentNode = parent.lock( ) ) {
		return parentNode->getTransformMatrix( ) * localTransform;
	} else {
		return localTransform;
	}
}

std::shared_ptr<Scene::Node> Scene::FindNodeByName( const std::string& name ) const {
	std::function<std::shared_ptr<Node>( const std::shared_ptr<Node>& )> searchNode =
		[&name, &searchNode]( const std::shared_ptr<Node>& node ) -> std::shared_ptr<Node> {
		if ( node->name == name ) {
			return node;
		}
		for ( const auto& child : node->children ) {
			auto result = searchNode( child );
			if ( result ) {
				return result;
			}
		}
		return nullptr;
	};

	for ( const auto& node : top_nodes ) {
		auto result = searchNode( node );
		if ( result ) {
			return result;
		}
	}

	return nullptr;
}
