#include "shader_storage.h"
#include <fstream>
#include <graphics/gfx_device.h>

#include <Windows.h>

FILETIME GetTimestamp( const char* path );

ShaderStorage::ShaderStorage( GfxDevice* gfx ) : gfx( gfx ) {}

void ShaderStorage::cleanup( ) {
	if ( shaders.empty( ) ) {
		return;
	}

	for ( auto& [key, shader] : shaders ) {
		vkDestroyShaderModule( gfx->device, shader.handle, nullptr );
	}

	shaders.clear( );
}

const Shader& ShaderStorage::Get( std::string name, ShaderType shader_type ) {
	std::string path = SHADER_PATH + name;
	if ( shader_type == ShaderType::T_COMPUTE ) {
		path += SHADER_COMP_EXT;
	} else if ( shader_type == ShaderType::T_VERTEX ) {
		path += SHADER_VERT_EXT;
	} else if ( shader_type == ShaderType::T_FRAGMENT ) {
		path += SHADER_FRAG_EXT;
	}

	if ( shaders.contains( path ) ) {
		return shaders[path];
	}

	Add( path, shader_type );

	return shaders[path];
}

void ShaderStorage::Reconstruct( ) {
	// TODO: file compilation will modify timestamp before it finishes compilation
	// file code ends up being empty 

	for ( auto& [key, shader] : shaders ) {
		FILETIME timestamp = GetTimestamp( shader.name.c_str( ) );
		if ( shader.low != timestamp.dwLowDateTime || shader.high != timestamp.dwHighDateTime ) {
			vkDestroyShaderModule( gfx->device, shader.handle, nullptr );

			auto handle = Shaders::LoadShaderModule( gfx->device, shader.name.c_str( ) );
			shader.handle = handle;
			shader.low = timestamp.dwLowDateTime;
			shader.high = timestamp.dwHighDateTime;

			fmt::println( "SHADER [{}] has been reloaded", shader.name.c_str( ) );

			shader.NotifyReload( );
		}
	}
}

void ShaderStorage::Add( std::string path, ShaderType shader_type ) {
	auto module = Shaders::LoadShaderModule( gfx->device, path.c_str( ) );
	if ( module == VK_NULL_HANDLE ) {
		throw "no shader module";
	}

	FILETIME timestamp = GetTimestamp( path.c_str( ) );
	shaders[path] = Shader( module, timestamp.dwLowDateTime, timestamp.dwHighDateTime, path );

	fmt::println( "[SHADER STORAGE]: Added {} shader", shaders[path].name.c_str( ) );
}

VkShaderModule Shaders::LoadShaderModule( VkDevice device, const char* path ) {
	std::ifstream file( path, std::ios::ate | std::ios::binary );
	if ( !file.is_open( ) ) {
		return VK_NULL_HANDLE;
	}

	size_t fileSize = (size_t)file.tellg( );
	std::vector<uint32_t> buffer( fileSize / sizeof( uint32_t ) );

	file.seekg( 0 );
	file.read( (char*)buffer.data( ), fileSize );
	file.close( );

	VkShaderModuleCreateInfo create_info = {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.pNext = nullptr,

		.codeSize = buffer.size( ) * sizeof( uint32_t ),
		.pCode = buffer.data( ),
	};

	VkShaderModule shader_module;
	if ( vkCreateShaderModule( device, &create_info, nullptr, &shader_module ) != VK_SUCCESS ) {
		return VK_NULL_HANDLE;
	}
	return shader_module;
}

FILETIME GetTimestamp( const char* path ) {
	auto handle = CreateFileA( path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr );
	if ( !handle ) {
		throw "no shader";
	}

	FILETIME timestamp;
	GetFileTime( handle, nullptr, nullptr, &timestamp );
	CloseHandle( handle );

	return timestamp;
}
