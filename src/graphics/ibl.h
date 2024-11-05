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

#include <graphics/pipelines/compute_pipeline.h>

class TL_VkContext;

namespace TL {
    class Ibl {
    public:
        void Init( TL_VkContext& gfx, const std::string& path );
        void Clean( const TL_VkContext& gfx );

        ImageId GetSkybox( ) const { return m_skybox; }
        ImageId GetIrradiance( ) const { return m_irradiance; }
        ImageId GetRadiance( ) const { return m_radiance; }
        ImageId GetBrdf( ) const { return m_brdf; }

    private:
        void InitComputes( TL_VkContext& gfx );
        void InitTextures( TL_VkContext& gfx );

        void GenerateSkybox( TL_VkContext& gfx, VkCommandBuffer cmd ) const;
        void GenerateIrradiance( TL_VkContext& gfx, VkCommandBuffer cmd ) const;
        void GenerateRadiance( TL_VkContext& gfx, VkCommandBuffer cmd ) const;
        void GenerateBrdf( TL_VkContext& gfx, VkCommandBuffer cmd ) const;

        VkCommandBuffer m_computeCommand = nullptr;
        VkFence         m_computeFence   = nullptr;

        ImageId m_hdrTexture = renderer::ImageCodex::InvalidImageId; // 2D

        ImageId m_skybox     = renderer::ImageCodex::InvalidImageId; // Cubemap
        ImageId m_irradiance = renderer::ImageCodex::InvalidImageId; // Cubemap
        ImageId m_radiance   = renderer::ImageCodex::InvalidImageId; // Cubemap
        ImageId m_brdf       = renderer::ImageCodex::InvalidImageId; // 2D

        BindlessCompute m_irradiancePipeline = { };
        VkDescriptorSet m_irradianceSet      = nullptr;

        struct RadiancePushConstants {
            ImageId input;
            int     mipmap;
            float   roughness;
        };
        BindlessCompute                m_radiancePipeline = { };
        std::array<VkDescriptorSet, 6> m_radianceSets     = { };

        BindlessCompute m_brdfPipeline = { };
        VkDescriptorSet m_brdfSet      = nullptr;
    };

} // namespace TL
