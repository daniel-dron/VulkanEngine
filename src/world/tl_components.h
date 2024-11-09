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

#include "graphics/resources/r_resources.h"
namespace TL::world {

    class Entity;

    enum class ComponentType : u32 {
        TRenderable = 0,
        TCamera,
        TLight,
        TDirectionalLight,
        TMax
    };

#define REGISTER_COMPONENT( T, EnumType ) \
    template<>                            \
    inline ComponentType BaseComponent::EnumToType<T>( ) { return EnumType; }

    // Base class of every single component in the engine world. Has a type and owner.
    // Any extending component can override its OnCreate, OnTick and OnDestroy virtual methods.
    class BaseComponent {
    public:
        explicit BaseComponent( Entity* owner ) :
            m_owner( owner ) {}

        virtual ~BaseComponent( ) = default;

        template<typename T>
        static constexpr ComponentType EnumToType( );

        virtual void OnCreate( ) {}  // Called when the component is created and added to an entity.
        virtual void OnStart( ) {}   // Called when the game/simulation is started.
        virtual void OnTick( ) {}    // Called every update tick.
        virtual void OnStop( ) {}    // Called when the game/simulation is stopped.
        virtual void OnDestroy( ) {} // Called when the component or entity is destroyed.

        ComponentType GetType( ) const { return m_type; }
        void          SetType( const ComponentType type ) { m_type = type; }

    protected:
        Entity*       m_owner = nullptr;
        ComponentType m_type  = ComponentType::TMax;
    };


    // Renderable Component. Used to represent a mesh in the engine.
    class Renderable final : public BaseComponent {
    public:
        Renderable( Entity* owner, renderer::MeshHandle mesh, renderer::MaterialHandle material ) noexcept;

        renderer::MeshHandle GetMeshHandle( ) const { return m_mesh; }
        renderer::MeshData&  GetMesh( ) const;
        void                 SetMesh( renderer::MeshHandle mesh );

        renderer::MaterialHandle GetMaterialHandle( ) const { return m_material; }
        renderer::MaterialData&  GetMaterial( ) const;
        void                     SetMaterial( renderer::MaterialHandle material );

    private:
        renderer::MeshHandle     m_mesh;
        renderer::MaterialHandle m_material;
    };
    REGISTER_COMPONENT( Renderable, ComponentType::TRenderable );

} // namespace TL::world