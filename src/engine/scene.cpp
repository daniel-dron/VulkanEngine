#include "scene.h"
#include <imgui.h>

void Scene::Node::propagateMatrix( ) {
	if ( this->parent.lock( ) != nullptr ) {
		transform.model = parent.lock( )->transform.model * transform.asMatrix( );
	} else {
		transform.model = transform.asMatrix( );
	}

	for ( auto&& node : children ) {
		node->propagateMatrix( );
	}
}