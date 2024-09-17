#include "scene.h"
#include <imgui.h>

void Scene::Node::propagateMatrix( ) {
	Transform3D parent_transform;

	if ( this->parent.lock( ) == nullptr ) {
		parent_transform = glm::mat4( 1.0f );
	} else {
		parent_transform = parent.lock( )->transform;
		
		this->transform = parent_transform * this->transform;
	}

	for ( auto& child : this->children ) {
		child->propagateMatrix( );
	}
}