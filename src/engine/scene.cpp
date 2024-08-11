#include "scene.h"

void Scene::Node::propagateMatrix( ) {
	if ( this->parent.lock( ) == nullptr ) {
		return;
	}

	this->transform = parent.lock( )->transform * this->transform;

	for ( auto& child : this->children ) {
		child->propagateMatrix( );
	}
}
