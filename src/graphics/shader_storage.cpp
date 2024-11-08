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

#include <filesystem>
#include <fstream>
#include <graphics/resources/r_resources.h>
#include "shader_storage.h"

#include "../engine/tl_engine.h"

using namespace std::filesystem;

std::time_t GetFileTimestamp( const std::string& file_path ) {
    path p( file_path );
    if ( exists( p ) ) {
        auto file_time = last_write_time( p );
        return std::chrono::duration_cast<std::chrono::seconds>( file_time.time_since_epoch( ) ).count( );
    }
    else {
        return 0; // Return 0 if file doesn't exist
    }
}

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
        auto timestamp = GetFileTimestamp( shader.name.c_str( ) );
        if ( shader.lastChangeTime != timestamp ) {
            vkDestroyShaderModule( m_gfx->device, shader.handle, nullptr );

            auto handle           = shaders::LoadShaderModule( m_gfx->device, shader.name.c_str( ) );
            shader.handle         = handle;
            shader.lastChangeTime = timestamp;

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

    const auto timestamp = GetFileTimestamp( name.c_str( ) );
    m_shaders[name]      = Shader( module, timestamp, name );

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