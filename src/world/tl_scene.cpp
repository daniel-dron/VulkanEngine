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

#include <optional>
#include <pch.h>

#include <memory>
#include "tl_entity.h"
#include "world/tl_scene.h"

#include "tl_scene.h"

namespace TL::world {

    EntityHandle World::CreateEntity( const std::string& name, EntityHandle parent ) {
        EntityHandle handle = { ( u32 )m_entities.size( ) };

        auto entity = std::make_shared<Entity>( name, handle, parent );

        m_entityList.emplace_back( entity );
        m_entities[handle] = entity;

        return handle;
    }

    void World::ObliterateEntity( EntityHandle entityHandle ) {
        assert( entityHandle != INVALID_ENTITY && "Trying to obliterate invalid entity" );
        assert( entityHandle != ROOT_ENTITY && "Root entity can not be obliterated" );

        if ( auto entity = GetEntity( entityHandle ).value( ) ) {
            m_toBeRemovedEntities.emplace_back( entityHandle );

            // Add its children too
            for ( auto& child : entity->GetChildren( ) ) {
                ObliterateEntity( child );
            }
        }
    }

    std::optional<Entity*> World::GetEntity( EntityHandle handle ) const {
        if ( m_entities.contains( handle ) ) {
            return m_entities.at( handle ).get( );
        }

        return std::nullopt;
    }


    void World::OnStart( ) {
        if ( m_alreadyStarted ) {
            return;
        }

        m_alreadyStarted = true;

        for ( auto& entity : m_entityList ) {
            entity->OnStart( );
        }
    }

    void World::OnTick( ) {
        for ( auto& entity : m_entityList ) {
            entity->OnTick( );
        }

        // Remove waiting to be removed entities
        for ( auto& entity_handle : m_toBeRemovedEntities ) {
            if ( auto entity = GetEntity( entity_handle ).value( ) ) {
                // Stop simulation for entity
                entity->OnStop( );

                // Remove this entity from its parent
                auto parent = GetEntity( entity->m_parent ).value( ); // Assume it has parent, always MUST have
                parent->RemoveChild( entity_handle );

                // Remove from entity list
                m_entityList.erase( std::remove_if( m_entityList.begin( ), m_entityList.end( ),
                                                    [entity]( const std::shared_ptr<Entity>& ptr ) {
                                                        return ptr.get( ) == entity;
                                                    } ),
                                    m_entityList.end( ) );

                // Remove from world
                m_entities.erase( entity_handle );

                // By now, its destructor should have been called
            }
        }
    }

    void World::OnStop( ) {
        if ( !m_alreadyStarted ) {
            return;
        }

        m_alreadyStarted = false;

        for ( auto& entity : m_entityList ) {
            entity->OnStop( );
        }
    }
} // namespace TL::world