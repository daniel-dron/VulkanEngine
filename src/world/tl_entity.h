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

#include "tl_components.h"

#include <algorithm>
#include <array>
#include <memory>

#include "tl_scene.h"

namespace TL::world {

    class World;

    template<typename T>
    concept ValidComponent = std::is_base_of_v<BaseComponent, T>;

    template<typename T, typename... Args>
    concept ConstructibleComponent = requires( Args&&... args ) {
        { T( nullptr, std::forward<Args>( args )... ) } -> std::same_as<T>;
    };

    struct EntityHandle {
        u32  index;
        bool operator==( const EntityHandle& ) const = default;
    };

    const EntityHandle INVALID_ENTITY = { 0 };

    class Entity {
    public:
        Entity( World* world, const std::string& name, EntityHandle handleToSelf, EntityHandle parentHandle );
        ~Entity( );

        // Creates and adds a new component to this entity. If a component of this type
        // already exists, the previous component is destroyed and removed
        template<ValidComponent T, typename... Args>
            requires ConstructibleComponent<T, Args...>
        void AddComponent( Args&&... args ) {
            const auto type = BaseComponent::EnumToType<T>( );

            // If the entity already has this component, then just destroy it and create a new one
            if ( std::shared_ptr<T> component = GetComponent<T>( ) ) {
                component->OnDestroy( );
                m_components[static_cast<u32>( type )].reset( );
            }

            std::shared_ptr<T> component           = std::make_shared<T>( this, std::forward<Args>( args )... );
            m_components[static_cast<u32>( type )] = component;

            component->OnCreate( );

            if ( m_alreadyStarted ) {
                component->OnStart( );
            }
        }

        // Tries to retreive a component of the given type.
        template<ValidComponent T>
        std::shared_ptr<T> GetComponent( ) {
            const auto type = BaseComponent::EnumToType<T>( );
            return dynamic_pointer_cast<T>( m_components[static_cast<u32>( type )] );
        }

        template<ValidComponent T>
        void DeleteComponent( ) {
            if ( std::shared_ptr<T> component = GetComponent<T>( ) ) {
                component->OnDestroy( );

                const auto type = BaseComponent::EnumToType<T>( );
                m_components[static_cast<u32>( type )].reset( );
            }
        }

        void AddChild( EntityHandle child );
        void RemoveChild( EntityHandle child );

        const std::vector<EntityHandle>& GetChildren( ) { return m_children; }

        EntityHandle GetHandle( ) const { return m_handle; }

        std::string Name;

    private:
        virtual void OnCreate( );
        virtual void OnStart( );
        virtual void OnTick( );
        virtual void OnStop( );
        virtual void OnDestroy( );

        std::array<std::shared_ptr<BaseComponent>, static_cast<u64>( ComponentType::TMax )> m_components;

        bool m_alreadyStarted = false; // Whether or not this entity has already been added to the world. If true, adding a new component will
                                       // implicitly call OnStart on the component.

        World*                    m_world;
        EntityHandle              m_handle = INVALID_ENTITY;
        std::vector<EntityHandle> m_children; // List of entity children. Will get destroyed when this entity is destroyed.
        EntityHandle              m_parent;   // Parent entity. When this entity is destroyed, parent will remove it from its children.

        friend class World;
    };

} // namespace TL::world

namespace std {
    template<>
    struct hash<TL::world::EntityHandle> {
        size_t operator( )( const TL::world::EntityHandle& h ) const {
            return hash<uint16_t>( )( h.index );
        }
    };
} // namespace std