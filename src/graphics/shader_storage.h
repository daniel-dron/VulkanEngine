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

#include <graphics/utils/vk_types.h>

#define SHADER_PATH "../shaders/"
#define SHADER_FRAG_EXT ".frag.spv"
#define SHADER_VERT_EXT ".vert.spv"
#define SHADER_COMP_EXT ".comp.spv"

struct Shader {
    using CallbackId     = std::uint64_t;
    using ReloadCallback = std::function<void( VkShaderModule module )>;

    VkShaderModule handle         = VK_NULL_HANDLE;
    std::time_t    lastChangeTime = 0;
    std::string    name;

    Shader( ) = default;
    Shader( const VkShaderModule shader, const std::time_t creationTime, const std::string& name ) :
        handle( shader ), lastChangeTime( creationTime ), name( name ), m_nextCallback( 0 ) {}

    // delete copy
    Shader( const Shader& )            = delete;
    Shader& operator=( const Shader& ) = delete;

    Shader( Shader&& other ) noexcept :
        handle( std::exchange( other.handle, VK_NULL_HANDLE ) ), lastChangeTime( other.lastChangeTime ), name( other.name ), m_nextCallback( 0 ) {}
    Shader& operator=( Shader&& other ) noexcept {
        if ( this != &other ) {
            handle         = std::exchange( other.handle, VK_NULL_HANDLE );
            lastChangeTime = other.lastChangeTime;
            name           = other.name;
        }

        return *this;
    }

    CallbackId RegisterReloadCallback( ReloadCallback callback ) const {
        const CallbackId id = m_nextCallback++;
        m_callbacks[id]     = std::move( callback );
        return id;
    }

    void UnregisterReloadCallback( const CallbackId id ) const {
        m_callbacks.erase( id );
    }

    void NotifyReload( ) const {
        for ( const auto& [id, callback] : m_callbacks ) {
            callback( handle );
        }
    }

private:
    mutable std::unordered_map<CallbackId, ReloadCallback> m_callbacks;
    mutable CallbackId                                     m_nextCallback;
};

enum ShaderType {
    TFragment,
    TVertex,
    TCompute
};

class ShaderStorage {
public:
    explicit ShaderStorage( TL_VkContext* gfx );
    void Cleanup( );

    void          Add( const std::string& name );
    const Shader& Get( const std::string& name, ShaderType shaderType );
    void          Reconstruct( );

private:
    std::unordered_map<std::string, Shader> m_shaders;
    TL_VkContext*                           m_gfx;
};

namespace shaders {
    VkShaderModule LoadShaderModule( VkDevice device, const char* path );
}
