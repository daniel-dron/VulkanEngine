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

#include <vk_types.h>

class TL_VkContext;

struct GpuMaterial {
    Vec4 baseColor;
    Vec4 metalRoughnessFactors;
    uint32_t colorTex;
    uint32_t metalRoughnessTex;
    uint32_t normalTex;
    uint32_t pad;
};

struct Material {
    Vec4 baseColor;
    float metalnessFactor;
    float roughnessFactor;

    ImageId colorId;
    ImageId metalRoughnessId;
    ImageId normalId;

    std::string name;
};

class MaterialCodex {
public:
    void Init( TL_VkContext &gfx );
    void Cleanup( TL_VkContext &gfx ) const;

    MaterialId AddMaterial( const TL_VkContext &gfx, const Material &material );
    const Material &GetMaterial( MaterialId id );

    const GpuBuffer &GetGpuBuffer( ) const { return m_materialGpu; }
    VkDeviceAddress GetDeviceAddress( ) const { return m_gpuAddress; }

private:
    GpuBuffer m_materialGpu = { };
    static constexpr size_t MaxMaterials = 1000;
    VkDeviceAddress m_gpuAddress = 0;

    std::vector<Material> m_materials;
};
