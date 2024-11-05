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
        void InitTextures( TL_VkContext& gfx );

        void GenerateSkybox( VkCommandBuffer cmd ) const;
        void GenerateIrradiance( VkCommandBuffer cmd ) const;
        void GenerateRadiance( VkCommandBuffer cmd ) const;
        void GenerateBrdf( VkCommandBuffer cmd ) const;

        VkCommandBuffer m_computeCommand = nullptr;
        VkFence         m_computeFence   = nullptr;

        ImageId m_hdrTexture = renderer::ImageCodex::InvalidImageId; // 2D

        ImageId m_skybox     = renderer::ImageCodex::InvalidImageId; // Cubemap
        ImageId m_irradiance = renderer::ImageCodex::InvalidImageId; // Cubemap
        ImageId m_radiance   = renderer::ImageCodex::InvalidImageId; // Cubemap
        ImageId m_brdf       = renderer::ImageCodex::InvalidImageId; // 2D
    };

} // namespace TL
