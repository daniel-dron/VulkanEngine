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

#include <array>
#include <memory>

namespace TL::world {

    template<typename T>
    concept ValidComponent = std::is_base_of_v<BaseComponent, T>;

    template<typename T, typename... Args>
    concept ConstructibleComponent = requires( Args&&... args ) {
        { T( nullptr, std::forward<Args>( args )... ) } -> std::same_as<T>;
    };

    class Entity {
    public:
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

    private:
        std::array<std::shared_ptr<BaseComponent>, static_cast<u64>( ComponentType::TMax )> m_components;
    };

} // namespace TL::world