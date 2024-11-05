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

#include <fstream>
#include <graphics/resources/r_resources.h>
#include "shader_storage.h"

#include <Windows.h>

#include "../engine/tl_engine.h"

FILETIME GetTimestamp( const char* path );

ShaderStorage::ShaderStorage( TL_VkContext* gfx ) :
    m_gfx( gfx ) {}

void ShaderStorage::Cleanup( ) {
    if ( m_shaders.empty( ) ) {
        return;
    }

    for ( auto& [key, shader] : m_shaders ) {
        vkDestroyShaderModule( m_gfx->device, shader.handle, nullptr );
    }

    m_shaders.clear( );
}

const Shader& ShaderStorage::Get( const std::string& name, const ShaderType shaderType ) {
    std::string path = SHADER_PATH + name;
    if ( shaderType == TCompute ) {
        path += SHADER_COMP_EXT;
    }
    else if ( shaderType == TVertex ) {
        path += SHADER_VERT_EXT;
    }
    else if ( shaderType == TFragment ) {
        path += SHADER_FRAG_EXT;
    }

    if ( m_shaders.contains( path ) ) {
        return m_shaders[path];
    }

    Add( path );

    return m_shaders[path];
}

void ShaderStorage::Reconstruct( ) {
    // TODO: file compilation will modify timestamp before it finishes compilation
    // file code ends up being empty

    for ( auto& [key, shader] : m_shaders ) {
        FILETIME timestamp = GetTimestamp( shader.name.c_str( ) );
        if ( shader.low != timestamp.dwLowDateTime || shader.high != timestamp.dwHighDateTime ) {
            vkDestroyShaderModule( m_gfx->device, shader.handle, nullptr );

            auto handle   = shaders::LoadShaderModule( m_gfx->device, shader.name.c_str( ) );
            shader.handle = handle;
            shader.low    = timestamp.dwLowDateTime;
            shader.high   = timestamp.dwHighDateTime;

            TL_Engine::Get( ).console.AddLog( "SHADER [{}] has been reloaded", shader.name.c_str( ) );

            shader.NotifyReload( );
        }
    }
}

void ShaderStorage::Add( const std::string& name ) {
    const auto module = shaders::LoadShaderModule( m_gfx->device, name.c_str( ) );
    if ( module == VK_NULL_HANDLE ) {
        throw "no shader module";
    }

    const FILETIME timestamp = GetTimestamp( name.c_str( ) );
    m_shaders[name]          = Shader( module, timestamp.dwLowDateTime, timestamp.dwHighDateTime, name );

    TL_Engine::Get( ).console.AddLog( "[SHADER STORAGE]: Added {} shader", m_shaders[name].name.c_str( ) );
}

VkShaderModule shaders::LoadShaderModule( const VkDevice device, const char* path ) {
    std::ifstream file( path, std::ios::ate | std::ios::binary );
    if ( !file.is_open( ) ) {
        return VK_NULL_HANDLE;
    }

    const size_t          file_size = file.tellg( );
    std::vector<uint32_t> buffer( file_size / sizeof( uint32_t ) );

    file.seekg( 0 );
    file.read( reinterpret_cast<char*>( buffer.data( ) ), file_size );
    file.close( );

    const VkShaderModuleCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext = nullptr,

            .codeSize = buffer.size( ) * sizeof( uint32_t ),
            .pCode    = buffer.data( ),
    };

    VkShaderModule shader_module;
    if ( vkCreateShaderModule( device, &create_info, nullptr, &shader_module ) != VK_SUCCESS ) {
        return VK_NULL_HANDLE;
    }
    return shader_module;
}

FILETIME GetTimestamp( const char* path ) {
    const auto handle =
            CreateFileA( path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr );
    if ( !handle ) {
        throw "no shader";
    }

    FILETIME timestamp;
    GetFileTime( handle, nullptr, nullptr, &timestamp );
    CloseHandle( handle );

    return timestamp;
}
