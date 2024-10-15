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

#include <graphics/image_codex.h>
#include <graphics/pipelines/compute_pipeline.h>
#include <vk_types.h>

class GfxDevice;

class Ibl {
public:
    void Init( GfxDevice &gfx, const std::string &path );
    void Clean( const GfxDevice &gfx ) ;

    ImageId GetSkybox( ) const { return m_skybox; }
    ImageId GetIrradiance( ) const { return m_irradiance; }
    ImageId GetRadiance( ) const { return m_radiance; }
    ImageId GetBrdf( ) const { return m_brdf; }

private:
    void InitComputes( GfxDevice &gfx );
    void InitTextures( GfxDevice &gfx );

    void GenerateSkybox( GfxDevice &gfx, VkCommandBuffer cmd ) const;
    void GenerateIrradiance( GfxDevice &gfx, VkCommandBuffer cmd ) const;
    void GenerateRadiance( GfxDevice &gfx, VkCommandBuffer cmd ) const;
    void GenerateBrdf( GfxDevice &gfx, VkCommandBuffer cmd ) const;

    VkCommandBuffer m_computeCommand = nullptr;
    VkFence m_computeFence = nullptr;

    ImageId m_hdrTexture = ImageCodex::InvalidImageId; // 2D

    ImageId m_skybox = ImageCodex::InvalidImageId; // Cubemap
    ImageId m_irradiance = ImageCodex::InvalidImageId; // Cubemap
    ImageId m_radiance = ImageCodex::InvalidImageId; // Cubemap
    ImageId m_brdf = ImageCodex::InvalidImageId; // 2D

    BindlessCompute m_irradiancePipeline = { };
    VkDescriptorSet m_irradianceSet = nullptr;

    BindlessCompute m_equirectangularPipeline = { };
    VkDescriptorSet m_equiSet = nullptr;

    struct RadiancePushConstants {
        ImageId input;
        int mipmap;
        float roughness;
    };
    BindlessCompute m_radiancePipeline = { };
    std::array<VkDescriptorSet, 6> m_radianceSets = { };

    BindlessCompute m_brdfPipeline = { };
    VkDescriptorSet m_brdfSet = nullptr;
};
