#include <pch.h>

#include "material_codex.h"

#include <graphics/gfx_device.h>

void MaterialCodex::init( GfxDevice& gfx ) {
	material_gpu = gfx.allocate( MAX_MATERIALS * sizeof( GpuMaterial ),
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VMA_MEMORY_USAGE_AUTO, "Material Data Bindless" );

	VkBufferDeviceAddressInfo address_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
		.pNext = nullptr,
		.buffer = material_gpu.buffer
	};
	gpu_address = vkGetBufferDeviceAddress( gfx.device, &address_info );
}

void MaterialCodex::cleanup( GfxDevice& gfx ) {
	gfx.free( material_gpu );
}

MaterialID MaterialCodex::addMaterial( GfxDevice& gfx, const Material& material ) {
	auto id = materials.size( );
	materials.push_back( material );

	GpuMaterial gpu_material = {};
	gpu_material.base_color = material.base_color;
	gpu_material.metal_roughness_factors = vec4( material.metalness_factor, material.roughness_factor, 0.0f, 0.0f );
	gpu_material.color_tex = material.color_id;
	gpu_material.metal_roughness_tex = material.metal_roughness_id;
	gpu_material.normal_tex = material.normal_id;

	GpuMaterial* gpu = nullptr;
	vmaMapMemory( gfx.allocator, material_gpu.allocation, (void**) & gpu );
	gpu[id] = gpu_material;
	vmaUnmapMemory( gfx.allocator, material_gpu.allocation );

	return id;
}

const Material& MaterialCodex::getMaterial( MaterialID id ) {
	return materials.at( id );
}
