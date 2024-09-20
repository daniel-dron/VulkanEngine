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
