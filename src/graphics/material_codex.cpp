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

#include "material_codex.h"

#include <graphics/gfx_device.h>

void MaterialCodex::Init( GfxDevice &gfx ) {
    m_materialGpu = gfx.Allocate( MaxMaterials * sizeof( GpuMaterial ), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_AUTO, "Material Data Bindless" );

    const VkBufferDeviceAddressInfo address_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            .pNext = nullptr,
            .buffer = m_materialGpu.buffer };
    m_gpuAddress = vkGetBufferDeviceAddress( gfx.device, &address_info );
}

void MaterialCodex::Cleanup( GfxDevice &gfx ) const {
    gfx.Free( m_materialGpu );
}

MaterialId MaterialCodex::AddMaterial( const GfxDevice &gfx, const Material &material ) {
    const auto id = m_materials.size( );
    m_materials.push_back( material );

    GpuMaterial gpu_material = { };
    gpu_material.baseColor = material.baseColor;
    gpu_material.metalRoughnessFactors = Vec4( material.metalnessFactor, material.roughnessFactor, 0.0f, 0.0f );
    gpu_material.colorTex = material.colorId;
    gpu_material.metalRoughnessTex = material.metalRoughnessId;
    gpu_material.normalTex = material.normalId;

    GpuMaterial *gpu = nullptr;
    vmaMapMemory( gfx.allocator, m_materialGpu.allocation, reinterpret_cast<void **>( &gpu ) );
    gpu[id] = gpu_material;
    vmaUnmapMemory( gfx.allocator, m_materialGpu.allocation );

    return id;
}

const Material &MaterialCodex::GetMaterial( const MaterialId id ) {
    return m_materials.at( id );
}
