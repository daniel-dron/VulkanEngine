#include "material_codex.h"

#include <graphics/gfx_device.h>

void MaterialCodex::init( GfxDevice& gfx ) {
	material_gpu = gfx.allocate( MAX_MATERIALS * sizeof( GpuMaterial ),
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VMA_MEMORY_USAGE_AUTO, "Material Data Bindless" );
}

void MaterialCodex::cleanup( GfxDevice& gfx ) {}

MaterialID MaterialCodex::addMaterial( GfxDevice& gfx, const Material& material ) {
	auto id = materials.size( );
	materials.push_back( material );

	return id;
}

const Material& MaterialCodex::getMaterial( MaterialID id ) {
	return materials.at( id );
}
