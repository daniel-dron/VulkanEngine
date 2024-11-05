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

#include <graphics/pipelines/compute_pipeline.h>
#include <graphics/resources/r_image.h>
#include <graphics/resources/r_resources.h>
#include <imgui.h>

#include <graphics/utils/vk_initializers.h>
#include "../engine/tl_engine.h"
#include "resources/r_pipeline.h"

using namespace TL::renderer;

namespace TL {
    void Ibl::Init( TL_VkContext& gfx, const std::string& path ) {
        m_hdrTexture = gfx.ImageCodex.LoadHdrFromFile( path, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT,
                                                       false );

        const VkCommandBufferAllocateInfo cmd_alloc_info =
                vk_init::CommandBufferAllocateInfo( gfx.computeCommandPool, 1 );
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
            auto cmd_info        = vk_init::CommandBufferSubmitInfo( m_computeCommand );
            auto cmd_submit_info = vk_init::SubmitInfo( &cmd_info, nullptr, nullptr );
            VKCALL( vkQueueSubmit2( gfx.computeQueue, 1, &cmd_submit_info, m_computeFence ) );
        }
    }

    void Ibl::Clean( const TL_VkContext& gfx ) {
        vkFreeCommandBuffers( gfx.device, gfx.computeCommandPool, 1, &m_computeCommand );
        vkDestroyFence( gfx.device, m_computeFence, nullptr );

        m_irradiancePipeline.Cleanup( gfx );
        m_radiancePipeline.Cleanup( gfx );
        m_brdfPipeline.Cleanup( gfx );
    }

    void Ibl::InitComputes( TL_VkContext& gfx ) {
        auto& irradiance_shader = gfx.shaderStorage->Get( "irradiance", TCompute );
        auto& radiance_shader   = gfx.shaderStorage->Get( "radiance", TCompute );
        auto& brdf_shader       = gfx.shaderStorage->Get( "brdf", TCompute );

        // ----------
        // Irradiance
        {
            m_irradiancePipeline.AddDescriptorSetLayout( 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE );
            m_irradiancePipeline.AddPushConstantRange( sizeof( ImageId ) );
            m_irradiancePipeline.Build( gfx, irradiance_shader.handle, "Irradiance Compute" );
            m_irradianceSet = gfx.AllocateSet( m_irradiancePipeline.GetLayout( ) );

            DescriptorWriter writer;
            auto&            irradiance_image = gfx.ImageCodex.GetImage( m_irradiance );
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

            auto& radiance_image = gfx.ImageCodex.GetImage( m_radiance );

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
            auto&            brdf_image = gfx.ImageCodex.GetImage( m_brdf );
            writer.WriteImage( 0, brdf_image.GetBaseView( ), nullptr, VK_IMAGE_LAYOUT_GENERAL,
                               VK_DESCRIPTOR_TYPE_STORAGE_IMAGE );
            writer.UpdateSet( gfx.device, m_brdfSet );
        }
    }

    void Ibl::InitTextures( TL_VkContext& gfx ) {
        VkImageUsageFlags usages{ };
        usages |= VK_IMAGE_USAGE_SAMPLED_BIT;
        usages |= VK_IMAGE_USAGE_STORAGE_BIT;

        m_skybox     = gfx.ImageCodex.CreateCubemap( "Skybox", VkExtent3D{ 2048, 2048, 1 }, VK_FORMAT_R32G32B32A32_SFLOAT, usages );
        m_irradiance = gfx.ImageCodex.CreateCubemap( "Irradiance", VkExtent3D{ 32, 32, 1 }, VK_FORMAT_R32G32B32A32_SFLOAT, usages );
        m_radiance   = gfx.ImageCodex.CreateCubemap( "Radiance", VkExtent3D{ 128, 128, 1 }, VK_FORMAT_R32G32B32A32_SFLOAT, usages, 6 );
        m_brdf       = gfx.ImageCodex.CreateEmptyImage( "BRDF", VkExtent3D{ 512, 512, 1 }, VK_FORMAT_R32G32B32A32_SFLOAT, usages );

        // Add all textures as storage images
        const auto& skybox     = vkctx->ImageCodex.GetImage( m_skybox );
        const auto& irradiance = vkctx->ImageCodex.GetImage( m_irradiance );
        const auto& radiance   = vkctx->ImageCodex.GetImage( m_radiance );
        const auto& brdf       = vkctx->ImageCodex.GetImage( m_brdf );

        vkctx->ImageCodex.bindlessRegistry.AddStorageImage( *vkctx, m_skybox, skybox.GetBaseView( ) );
        vkctx->ImageCodex.bindlessRegistry.AddStorageImage( *vkctx, m_irradiance, irradiance.GetBaseView( ) );
        vkctx->ImageCodex.bindlessRegistry.AddStorageImage( *vkctx, m_radiance, radiance.GetBaseView( ) );
        vkctx->ImageCodex.bindlessRegistry.AddStorageImage( *vkctx, m_brdf, brdf.GetBaseView( ) );
    }

    void Ibl::GenerateSkybox( TL_VkContext& gfx, const VkCommandBuffer cmd ) const {
        const struct PushConstants {
            ImageId Input;
            ImageId Output;
        } pc = {
                .Input  = m_hdrTexture,
                .Output = m_skybox };

        auto pipeline = vkctx->GetOrCreatePipeline( PipelineConfig{
                .name                 = "equirectangular",
                .compute              = "../shaders/equirectangular_map.comp.spv",
                .pushConstantRanges   = { { .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                                            .offset     = 0,
                                            .size       = sizeof( PushConstants ) } },
                .descriptorSetLayouts = { vkctx->GetBindlessLayout( ) },
        } );

        const auto& output = vkctx->ImageCodex.GetImage( m_skybox );
        output.TransitionLayout( cmd, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL );

        vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->GetVkResource( ) );

        const auto bindless_set = vkctx->GetBindlessSet( );
        vkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->GetLayout( ), 0, 1, &bindless_set, 0, nullptr );

        vkCmdPushConstants( cmd, pipeline->GetLayout( ), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( PushConstants ), &pc );
        vkCmdDispatch( cmd, ( output.GetExtent( ).width + 15 ) / 16, ( output.GetExtent( ).height + 15 ) / 16, 6 );
    }

    void Ibl::GenerateIrradiance( TL_VkContext& gfx, const VkCommandBuffer cmd ) const {
        const auto bindless = gfx.GetBindlessSet( );
        const auto input    = m_skybox;
        const auto output   = m_irradiance;

        auto& output_image = gfx.ImageCodex.GetImage( output );
        m_irradiancePipeline.Bind( cmd );
        m_irradiancePipeline.BindDescriptorSet( cmd, bindless, 0 );
        m_irradiancePipeline.BindDescriptorSet( cmd, m_irradianceSet, 1 );
        m_irradiancePipeline.PushConstants( cmd, sizeof( ImageId ), &input );
        m_irradiancePipeline.Dispatch( cmd, ( output_image.GetExtent( ).width + 15 ) / 16,
                                       ( output_image.GetExtent( ).height + 15 ) / 16, 6 );
    }

    void Ibl::GenerateRadiance( TL_VkContext& gfx, const VkCommandBuffer cmd ) const {
        const auto bindless = gfx.GetBindlessSet( );
        const auto input    = m_skybox;
        const auto output   = m_radiance;

        auto& output_image = gfx.ImageCodex.GetImage( output );

        RadiancePushConstants pc = {
                .input     = input,
                .mipmap    = 0,
                .roughness = 0,
        };

        m_radiancePipeline.Bind( cmd );
        m_radiancePipeline.BindDescriptorSet( cmd, bindless, 0 );

        for ( auto mip = 0; mip < 6; mip++ ) {
            const float roughness = static_cast<float>( mip ) / static_cast<float>( 6 - 1 );

            pc.mipmap    = mip;
            pc.roughness = roughness;

            const auto set = m_radianceSets[mip];

            m_radiancePipeline.BindDescriptorSet( cmd, set, 1 );
            m_radiancePipeline.PushConstants( cmd, sizeof( RadiancePushConstants ), &pc );
            m_radiancePipeline.Dispatch( cmd, ( output_image.GetExtent( ).width + 15 ) / 16,
                                         ( output_image.GetExtent( ).height + 15 ) / 16, 6 );
        }
    }

    void Ibl::GenerateBrdf( TL_VkContext& gfx, const VkCommandBuffer cmd ) const {
        const auto bindless     = gfx.GetBindlessSet( );
        const auto output       = m_brdf;
        auto&      output_image = gfx.ImageCodex.GetImage( output );

        image::TransitionLayout( cmd, output_image.GetImage( ), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL );

        m_brdfPipeline.Bind( cmd );
        m_brdfPipeline.BindDescriptorSet( cmd, bindless, 0 );
        m_brdfPipeline.BindDescriptorSet( cmd, m_brdfSet, 1 );
        m_brdfPipeline.Dispatch( cmd, ( output_image.GetExtent( ).width + 15 ) / 16,
                                 ( output_image.GetExtent( ).height + 15 ) / 16, 1 );
    }

} // namespace TL
