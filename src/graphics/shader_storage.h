#pragma once

#include <vk_types.h>

#define SHADER_PATH "../../shaders/"
#define SHADER_FRAG_EXT ".frag.spv"
#define SHADER_VERT_EXT ".vert.spv"
#define SHADER_COMP_EXT ".comp.spv"

struct Shader {
	VkShaderModule handle = VK_NULL_HANDLE;
	unsigned long low;
	unsigned long high;
	std::string name;

	Shader( ) = default;
	Shader( VkShaderModule shader, unsigned long low, unsigned long high, const std::string& name )
		: handle( shader ), low( low ), high( high ), name( name ) {}

	// delte copy
	Shader( const Shader& ) = delete;
	Shader& operator=( const Shader& ) = delete;

	Shader( Shader&& other ) noexcept
		: handle( std::exchange( other.handle, VK_NULL_HANDLE ) )
		, low( other.low )
		, high( other.high )
		, name( other.name ) {}
	Shader& operator=( Shader&& other ) noexcept {
		if ( this != &other ) {
			handle = std::exchange( other.handle, VK_NULL_HANDLE );
			low = other.low;
			high = other.high;
			name = other.name;
		}

		return *this;
	}
};

enum ShaderType {
	T_FRAGMENT,
	T_VERTEX,
	T_COMPUTE
};

class ShaderStorage {
public:
	ShaderStorage( GfxDevice* gfx );
	void cleanup( );

	const Shader& Get( std::string name, ShaderType shader_type );
private:
	void Add( std::string name, ShaderType shader_type );

	std::unordered_map<std::string, Shader> shaders;
	GfxDevice* gfx;
};

namespace Shaders {
	VkShaderModule LoadShaderModule( VkDevice device, const char* path );
}