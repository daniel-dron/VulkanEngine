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

#include "tl_components.h"

namespace TL::world {

    using namespace renderer;

    template<typename T>
    inline constexpr ComponentType BaseComponent::EnumToType( ) { return ComponentType::TMax; }

    Renderable::Renderable( Entity* owner, const MeshHandle mesh, const MaterialHandle material ) noexcept :
        BaseComponent( owner ), m_mesh( mesh ), m_material( material ) {
        m_type = ComponentType::TRenderable;
    }

    MeshData& Renderable::GetMesh( ) const {
        return vkctx->MeshPool.GetMesh( m_mesh );
    }

    void Renderable::SetMesh( const MeshHandle mesh ) {
        assert( vkctx->MeshPool.IsValid( mesh ) && "Trying to set invalid mesh on renderable component" );
        m_mesh = mesh;
    }

    MaterialData& Renderable::GetMaterial( ) const {
        return vkctx->MaterialPool.GetMaterial( m_material );
    }

    void Renderable::SetMaterial( const MaterialHandle material ) {
        assert( vkctx->MaterialPool.IsValid( material ) && "Trying to set invalid material on renderable component" );
        m_material = material;
    }

} // namespace TL::world