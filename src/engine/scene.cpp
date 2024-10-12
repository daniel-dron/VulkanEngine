/******************************************************************************
******************************************************************************
**                                                                           **
**                             Twilight Engine                               **
**                                                                           **
**                  Copyright (c) 2024-present Daniel Dron                   **
**                                                                           **
**            This software is released under the MIT License.               **
**                 https://opensource.org/licenses/MIT                       **
**                                                                           **
******************************************************************************
******************************************************************************/

#include <pch.h>

#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include "scene.h"

void Node::SetTransform( const Mat4 &newTransform ) {
    glm::vec3 skew{ };
    glm::vec4 perspective{ };
    glm::quat rotation{ };

    decompose( newTransform, transform.scale, rotation, transform.position, skew, perspective );

    extractEulerAngleXYZ( glm::mat4_cast( rotation ), transform.euler.x, transform.euler.y, transform.euler.z );

    transform.model = newTransform;
}

Mat4 Node::GetTransformMatrix( ) const {
    const Mat4 local_transform = transform.AsMatrix( );

    if ( const auto parent_node = parent.lock( ) ) {
        return parent_node->GetTransformMatrix( ) * local_transform;
    }

    return local_transform;
}

std::shared_ptr<Node> Scene::FindNodeByName( const std::string &name ) const {
    std::function<std::shared_ptr<Node>( const std::shared_ptr<Node> & )> search_node = [&name, &search_node]( const std::shared_ptr<Node> &node ) -> std::shared_ptr<Node> {
        if ( node->name == name ) {
            return node;
        }
        for ( const auto &child : node->children ) {
            auto result = search_node( child );
            if ( result ) {
                return result;
            }
        }
        return nullptr;
    };

    for ( const auto &node : topNodes ) {
        auto result = search_node( node );
        if ( result ) {
            return result;
        }
    }

    return nullptr;
}
