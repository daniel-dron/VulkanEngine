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

#include "ibl.h"

#include <graphics/image_codex.h>
#include <graphics/tl_vkcontext.h>

#include <graphics/pipelines/compute_pipeline.h>
#include <imgui.h>
#include <vk_pipelines.h>

#include "vk_engine.h"
#include "vk_initializers.h"

void Ibl::Init( TL_VkContext &gfx, const std::string &path ) {
    m_hdrTexture =
            gfx.imageCodex.LoadHdrFromFile( path, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT, false );

    const VkCommandBufferAllocateInfo cmd_alloc_info = vk_init::CommandBufferAllocateInfo( gfx.computeCommandPool, 1 );
    VKCALL( vkAllocateCommandBuffers( gfx.device, &cmd_alloc_info, &m_computeCommand ) );

    const auto fence_create_info = vk_init::FenceCreateInfo( );
    VKCALL( vkCreateFence( gfx.device, &fence_create_info, nullptr, &m_computeFence ) );

    InitTextures( gfx );

    InitComputes( gfx );

    TL_Engine::Get( ).console.AddLog( "Dispatching IBL computes!" );
    // dispatch computes
    {
        VKCALL( vkResetCommandBuffer( m_computeCommand, 0 ) );
        const auto cmd_begin_info = vk_init::CommandBufferBeginInfo( VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT );
        VKCALL( vkBeginCommandBuffer( m_computeCommand, &cmd_begin_info ) );

        GenerateSkybox( gfx, m_computeCommand );
        GenerateIrradiance( gfx, m_computeCommand );
        GenerateRadiance( gfx, m_computeCommand );
        GenerateBrdf( gfx, m_computeCommand );

        VKCALL( vkEndCommandBuffer( m_computeCommand ) );
        auto cmd_info = vk_init::CommandBufferSubmitInfo( m_computeCommand );
        auto cmd_submit_info = vk_init::SubmitInfo( &cmd_info, nullptr, nullptr );
        VKCALL( vkQueueSubmit2( gfx.computeQueue, 1, &cmd_submit_info, m_computeFence ) );
    }
}

void Ibl::Clean( const TL_VkContext &gfx ) {
    vkFreeCommandBuffers( gfx.device, gfx.computeCommandPool, 1, &m_computeCommand );
    vkDestroyFence( gfx.device, m_computeFence, nullptr );

    m_equirectangularPipeline.Cleanup( gfx );
    m_irradiancePipeline.Cleanup( gfx );
    m_radiancePipeline.Cleanup( gfx );
    m_brdfPipeline.Cleanup( gfx );
}

void Ibl::InitComputes( TL_VkContext &gfx ) {
    auto &equirectangular_shader = gfx.shaderStorage->Get( "equirectangular_map", TCompute );
    auto &irradiance_shader = gfx.shaderStorage->Get( "irradiance", TCompute );
    auto &radiance_shader = gfx.shaderStorage->Get( "radiance", TCompute );
    auto &brdf_shader = gfx.shaderStorage->Get( "brdf", TCompute );

    // Equirectangular to Cubemap
    {
        m_equirectangularPipeline.AddDescriptorSetLayout( 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE );
        m_equirectangularPipeline.AddPushConstantRange( sizeof( ImageId ) );
        m_equirectangularPipeline.Build( gfx, equirectangular_shader.handle, "Equirectangular to Cubemap Pipeline" );
        m_equiSet = gfx.AllocateSet( m_equirectangularPipeline.GetLayout( ) );

        DescriptorWriter writer;
        auto &skybox_image = gfx.imageCodex.GetImage( m_skybox );
        writer.WriteImage( 0, skybox_image.GetBaseView( ), nullptr, VK_IMAGE_LAYOUT_GENERAL,
                           VK_DESCRIPTOR_TYPE_STORAGE_IMAGE );
        writer.UpdateSet( gfx.device, m_equiSet );
    }

    // ----------
    // Irradiance
    {
        m_irradiancePipeline.AddDescriptorSetLayout( 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE );
        m_irradiancePipeline.AddPushConstantRange( sizeof( ImageId ) );
        m_irradiancePipeline.Build( gfx, irradiance_shader.handle, "Irradiance Compute" );
        m_irradianceSet = gfx.AllocateSet( m_irradiancePipeline.GetLayout( ) );

        DescriptorWriter writer;
        auto &irradiance_image = gfx.imageCodex.GetImage( m_irradiance );
        writer.WriteImage( 0, irradiance_image.GetBaseView( ), nullptr, VK_IMAGE_LAYOUT_GENERAL,
                           VK_DESCRIPTOR_TYPE_STORAGE_IMAGE );
        writer.UpdateSet( gfx.device, m_irradianceSet );
    }

    // ----------
    // Radiance
    {
        m_radiancePipeline.AddDescriptorSetLayout( 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE );
        m_radiancePipeline.AddPushConstantRange( sizeof( RadiancePushConstants ) );
        m_radiancePipeline.Build( gfx, radiance_shader.handle, "Radiance Compute" );

        auto &radiance_image = gfx.imageCodex.GetImage( m_radiance );

        for ( auto i = 0; i < 6; i++ ) {
            m_radianceSets[i] = gfx.AllocateSet( m_radiancePipeline.GetLayout( ) );

            DescriptorWriter writer;
            writer.WriteImage( 0, radiance_image.GetMipView( i ), nullptr, VK_IMAGE_LAYOUT_GENERAL,
                               VK_DESCRIPTOR_TYPE_STORAGE_IMAGE );
            writer.UpdateSet( gfx.device, m_radianceSets[i] );
        }
    }

    // ----------
    // BRDF
    {
        m_brdfPipeline.AddDescriptorSetLayout( 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE );
        m_brdfPipeline.Build( gfx, brdf_shader.handle, "BRDF Compute" );
        m_brdfSet = gfx.AllocateSet( m_brdfPipeline.GetLayout( ) );

        DescriptorWriter writer;
        auto &brdf_image = gfx.imageCodex.GetImage( m_brdf );
        writer.WriteImage( 0, brdf_image.GetBaseView( ), nullptr, VK_IMAGE_LAYOUT_GENERAL,
                           VK_DESCRIPTOR_TYPE_STORAGE_IMAGE );
        writer.UpdateSet( gfx.device, m_brdfSet );
    }
}

void Ibl::InitTextures( TL_VkContext &gfx ) {
    VkImageUsageFlags usages{ };
    usages |= VK_IMAGE_USAGE_SAMPLED_BIT;
    usages |= VK_IMAGE_USAGE_STORAGE_BIT;

    m_skybox = gfx.imageCodex.CreateCubemap( "Skybox", VkExtent3D{ 2048, 2048, 1 }, VK_FORMAT_R32G32B32A32_SFLOAT,
                                             usages );
    m_irradiance = gfx.imageCodex.CreateCubemap( "Irradiance", VkExtent3D{ 32, 32, 1 }, VK_FORMAT_R32G32B32A32_SFLOAT,
                                                 usages );
    m_radiance = gfx.imageCodex.CreateCubemap( "Radiance", VkExtent3D{ 128, 128, 1 }, VK_FORMAT_R32G32B32A32_SFLOAT,
                                               usages, 6 );
    m_brdf =
            gfx.imageCodex.CreateEmptyImage( "BRDF", VkExtent3D{ 512, 512, 1 }, VK_FORMAT_R32G32B32A32_SFLOAT, usages );
}

void Ibl::GenerateSkybox( TL_VkContext &gfx, const VkCommandBuffer cmd ) const {
    const auto bindless = gfx.GetBindlessSet( );
    const auto input = m_hdrTexture;
    const auto output = m_skybox;

    auto &output_image = gfx.imageCodex.GetImage( output );

    m_equirectangularPipeline.Bind( cmd );
    m_equirectangularPipeline.BindDescriptorSet( cmd, bindless, 0 );
    m_equirectangularPipeline.BindDescriptorSet( cmd, m_equiSet, 1 );
    m_equirectangularPipeline.PushConstants( cmd, sizeof( ImageId ), &input );
    m_equirectangularPipeline.Dispatch( cmd, ( output_image.GetExtent( ).width + 15 ) / 16,
                                        ( output_image.GetExtent( ).height + 15 ) / 16, 6 );
}

void Ibl::GenerateIrradiance( TL_VkContext &gfx, const VkCommandBuffer cmd ) const {
    const auto bindless = gfx.GetBindlessSet( );
    const auto input = m_skybox;
    const auto output = m_irradiance;

    auto &output_image = gfx.imageCodex.GetImage( output );
    m_irradiancePipeline.Bind( cmd );
    m_irradiancePipeline.BindDescriptorSet( cmd, bindless, 0 );
    m_irradiancePipeline.BindDescriptorSet( cmd, m_irradianceSet, 1 );
    m_irradiancePipeline.PushConstants( cmd, sizeof( ImageId ), &input );
    m_irradiancePipeline.Dispatch( cmd, ( output_image.GetExtent( ).width + 15 ) / 16,
                                   ( output_image.GetExtent( ).height + 15 ) / 16, 6 );
}

void Ibl::GenerateRadiance( TL_VkContext &gfx, const VkCommandBuffer cmd ) const {
    const auto bindless = gfx.GetBindlessSet( );
    const auto input = m_skybox;
    const auto output = m_radiance;

    auto &output_image = gfx.imageCodex.GetImage( output );

    RadiancePushConstants pc = {
            .input = input,
            .mipmap = 0,
            .roughness = 0,
    };

    m_radiancePipeline.Bind( cmd );
    m_radiancePipeline.BindDescriptorSet( cmd, bindless, 0 );

    for ( auto mip = 0; mip < 6; mip++ ) {
        const float roughness = static_cast<float>( mip ) / static_cast<float>( 6 - 1 );

        pc.mipmap = mip;
        pc.roughness = roughness;

        const auto set = m_radianceSets[mip];

        m_radiancePipeline.BindDescriptorSet( cmd, set, 1 );
        m_radiancePipeline.PushConstants( cmd, sizeof( RadiancePushConstants ), &pc );
        m_radiancePipeline.Dispatch( cmd, ( output_image.GetExtent( ).width + 15 ) / 16,
                                     ( output_image.GetExtent( ).height + 15 ) / 16, 6 );
    }
}

void Ibl::GenerateBrdf( TL_VkContext &gfx, const VkCommandBuffer cmd ) const {
    const auto bindless = gfx.GetBindlessSet( );
    const auto output = m_brdf;
    auto &output_image = gfx.imageCodex.GetImage( output );

    image::TransitionLayout( cmd, output_image.GetImage( ), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                             VK_IMAGE_LAYOUT_GENERAL );

    m_brdfPipeline.Bind( cmd );
    m_brdfPipeline.BindDescriptorSet( cmd, bindless, 0 );
    m_brdfPipeline.BindDescriptorSet( cmd, m_brdfSet, 1 );
    m_brdfPipeline.Dispatch( cmd, ( output_image.GetExtent( ).width + 15 ) / 16,
                             ( output_image.GetExtent( ).height + 15 ) / 16, 1 );
}
