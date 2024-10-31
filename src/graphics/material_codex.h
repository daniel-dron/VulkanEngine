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

#include <graphics/resources/tl_buffer.h>
#include <vk_types.h>

namespace TL {
    struct Material {
        Vec4  baseColor;
        float metalnessFactor;
        float roughnessFactor;

        ImageId colorId;
        ImageId metalRoughnessId;
        ImageId normalId;

        std::string name;
    };
    struct GpuMaterial {
        Vec4     baseColor;
        Vec4     metalRoughnessFactors;
        uint32_t colorTex;
        uint32_t metalRoughnessTex;
        uint32_t normalTex;
        uint32_t pad;
    };

    class MaterialCodex {
    public:
        void Init( );
        void Cleanup( );

        MaterialId      AddMaterial( const Material &material );
        const Material &GetMaterial( MaterialId id );

        const Buffer   &GetGpuBuffer( ) const { return *m_materialGpu; }
        VkDeviceAddress GetDeviceAddress( ) const { return m_materialGpu->GetDeviceAddress( ); }

    private:
        std::unique_ptr<Buffer> m_materialGpu = nullptr;
        static constexpr size_t MaxMaterials  = 1000;

        std::array<Material, MaxMaterials> m_materials      = { };
        u32                                m_nextMaterialId = 0;
    };
} // namespace TL