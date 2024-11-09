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

#include "tl_entity.h"
#include "tl_scene.h"
#include "world/tl_entity.h"

namespace TL::world {
    Entity::Entity( World* world, const std::string& name, EntityHandle handleToSelfe, EntityHandle parentHandle ) :
        Name( name ), m_world( world ), m_handle( handleToSelfe ), m_parent( parentHandle ) {
        OnCreate( );
    }

    Entity::~Entity( ) {
        OnDestroy( );
    }

    void Entity::OnCreate( ) {
    }

    void Entity::OnStart( ) {
        m_alreadyStarted = true;

        for ( auto& component : m_components ) {
            if ( component ) {
                component->OnStart( );
            }
        }
    }

    void Entity::OnTick( ) {
        for ( auto& component : m_components ) {
            if ( component ) {
                component->OnTick( );
            }
        }
    }

    void Entity::OnStop( ) {
        // If the entity hasnt even started, no need to do anything
        if ( !m_alreadyStarted ) {
            return;
        }

        m_alreadyStarted = false;

        for ( auto& component : m_components ) {
            if ( component ) {
                component->OnStop( );
            }
        }
    }

    void Entity::OnDestroy( ) {
        // Destroy all components and reset their pointers to not cause use-after-free
        for ( auto& component : m_components ) {
            if ( component ) {
                component->OnDestroy( );
                component.reset( );
            }

            component = nullptr;
        }
    }

    void Entity::AddChild( EntityHandle child ) {
        // Check if already children
        if ( std::find( m_children.begin( ), m_children.end( ), child ) != m_children.end( ) ) {
            return;
        }

        // Set this entity as its parent
        if ( auto entity = m_world->GetEntity( child ).value( ) ) {

            // Remove from old parent
            if ( auto old_parent = m_world->GetEntity( entity->m_parent ).value( ) ) {
                old_parent->RemoveChild( child );
            }

            entity->m_parent = m_handle;
        }

        m_children.emplace_back( child );
    };

    void Entity::RemoveChild( EntityHandle child ) {
        auto found = std::find( m_children.begin( ), m_children.end( ), child );
        if ( found != m_children.end( ) ) {
            if ( auto entity = m_world->GetEntity( child ).value( ) ) {
                entity->m_parent = INVALID_ENTITY;
            }
            m_children.erase( found );
        }
    }

    void Entity::SetTransform( const Mat4& newTransform ) {
        glm::vec3 skew{ };
        glm::vec4 perspective{ };
        glm::quat rotation{ };

        decompose( newTransform, Transform.scale, rotation, Transform.position, skew, perspective );

        extractEulerAngleXYZ( glm::mat4_cast( rotation ), Transform.euler.x, Transform.euler.y, Transform.euler.z );

        Transform.model = newTransform;
    }

    Mat4 Entity::GetTransformMatrix( ) const {
        const Mat4 local_transform = Transform.AsMatrix( );

        if ( m_parent != INVALID_ENTITY ) {
            if ( auto parent = m_world->GetEntity( m_parent ).value( ) ) {
                return parent->GetTransformMatrix( ) * local_transform;
            }
        }

        return local_transform;
    }
} // namespace TL::world