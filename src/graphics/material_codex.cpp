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

namespace TL {
    void MaterialCodex::Init( ) {
        m_materialGpu = std::make_unique<Buffer>( BufferType::TStorage, MaxMaterials * sizeof( GpuMaterial ), 1,
                                                  nullptr, "[TL] Materials" );
    }

    void MaterialCodex::Cleanup( ) { m_materialGpu.reset( ); }

    MaterialId MaterialCodex::AddMaterial( const Material &material ) {
        const MaterialId id = m_nextMaterialId;
        m_nextMaterialId++;
        m_materials[id] = material;

        GpuMaterial gpu_material           = { };
        gpu_material.baseColor             = material.baseColor;
        gpu_material.metalRoughnessFactors = Vec4( material.metalnessFactor, material.roughnessFactor, 0.0f, 0.0f );
        gpu_material.colorTex              = material.colorId;
        gpu_material.metalRoughnessTex     = material.metalRoughnessId;
        gpu_material.normalTex             = material.normalId;

        m_materialGpu->Upload( &gpu_material, id * sizeof( GpuMaterial ), sizeof( GpuMaterial ) );
        return id;
    }

    const Material &MaterialCodex::GetMaterial( const MaterialId id ) { return m_materials.at( id ); }
} // namespace TL
