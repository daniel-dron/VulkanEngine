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
#pragma once

#include "tl_entity.h"

#include <memory>
#include <unordered_map>
#include <vector>

namespace TL::world {

    const EntityHandle ROOT_ENTITY = { 1 };

    class World {
    public:
        World( );

        void OnStart( );
        void OnTick( );
        void OnStop( );

        // Create a new entity for the given parent. If parent is null, the entity will be created under the root entity.
        EntityHandle           CreateEntity( const std::string& name, EntityHandle parent = ROOT_ENTITY );
        void                   ObliterateEntity( EntityHandle entity );
        std::optional<Entity*> GetEntity( EntityHandle handle ) const;
        bool                   IsValidEntity( EntityHandle handle ) const;

    private:
        bool m_alreadyStarted = false;

        std::unordered_map<EntityHandle, std::shared_ptr<Entity>> m_entities;
        std::vector<std::shared_ptr<Entity>>                      m_entityList;          // Vector that keeps track of all entities created
        std::shared_ptr<Entity>                                   m_rootEntity;          // World root entity. All entities should be part of this hierarchy.
        std::vector<EntityHandle>                                 m_toBeRemovedEntities; // Entities that will be removed/destroyed at the end of tick.
    };
} // namespace TL::world