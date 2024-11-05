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

        TL_Engine::Get( ).console.AddLog( "Dispatching IBL computes!" );
        // dispatch computes
        {
            VKCALL( vkResetCommandBuffer( m_computeCommand, 0 ) );
            const auto cmd_begin_info = vk_init::CommandBufferBeginInfo( VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT );
            VKCALL( vkBeginCommandBuffer( m_computeCommand, &cmd_begin_info ) );

            GenerateSkybox( m_computeCommand );
            GenerateIrradiance( m_computeCommand );
            GenerateRadiance( m_computeCommand );
            GenerateBrdf( m_computeCommand );

            VKCALL( vkEndCommandBuffer( m_computeCommand ) );
            auto cmd_info        = vk_init::CommandBufferSubmitInfo( m_computeCommand );
            auto cmd_submit_info = vk_init::SubmitInfo( &cmd_info, nullptr, nullptr );
            VKCALL( vkQueueSubmit2( gfx.computeQueue, 1, &cmd_submit_info, m_computeFence ) );
        }
    }

    void Ibl::Clean( const TL_VkContext& gfx ) {
        vkFreeCommandBuffers( gfx.device, gfx.computeCommandPool, 1, &m_computeCommand );
        vkDestroyFence( gfx.device, m_computeFence, nullptr );
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

    void Ibl::GenerateSkybox( const VkCommandBuffer cmd ) const {
        const struct PushConstants {
            ImageId Input;
            ImageId Output;
        } pc = { .Input  = m_hdrTexture,
                 .Output = m_skybox };

        static auto pipeline_config = PipelineConfig{
                .name                 = "equirectangular",
                .compute              = "../shaders/equirectangular_map.comp.spv",
                .pushConstantRanges   = { { .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                                            .offset     = 0,
                                            .size       = sizeof( PushConstants ) } },
                .descriptorSetLayouts = { vkctx->GetBindlessLayout( ) } };
        const auto pipeline = vkctx->GetOrCreatePipeline( pipeline_config );

        const auto& output = vkctx->ImageCodex.GetImage( m_skybox );
        output.TransitionLayout( cmd, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL );

        vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->GetVkResource( ) );

        const auto bindless_set = vkctx->GetBindlessSet( );
        vkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->GetLayout( ), 0, 1, &bindless_set, 0, nullptr );

        vkCmdPushConstants( cmd, pipeline->GetLayout( ), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( PushConstants ), &pc );
        vkCmdDispatch( cmd, ( output.GetExtent( ).width + 15 ) / 16, ( output.GetExtent( ).height + 15 ) / 16, 6 );
    }

    void Ibl::GenerateIrradiance( const VkCommandBuffer cmd ) const {
        const struct PushConstants {
            ImageId Input;
            ImageId Output;
        } pc = { .Input  = m_skybox,
                 .Output = m_irradiance };

        static auto pipeline_config = PipelineConfig{
                .name                 = "irradiance",
                .compute              = "../shaders/irradiance.comp.spv",
                .pushConstantRanges   = { { .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                                            .offset     = 0,
                                            .size       = sizeof( PushConstants ) } },
                .descriptorSetLayouts = { vkctx->GetBindlessLayout( ) } };
        const auto pipeline = vkctx->GetOrCreatePipeline( pipeline_config );

        const auto& output = vkctx->ImageCodex.GetImage( m_irradiance );
        output.TransitionLayout( cmd, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL );

        vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->GetVkResource( ) );

        const auto bindless_set = vkctx->GetBindlessSet( );
        vkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->GetLayout( ), 0, 1, &bindless_set, 0, nullptr );

        vkCmdPushConstants( cmd, pipeline->GetLayout( ), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( PushConstants ), &pc );
        vkCmdDispatch( cmd, ( output.GetExtent( ).width + 15 ) / 16, ( output.GetExtent( ).height + 15 ) / 16, 6 );
    }

    void Ibl::GenerateRadiance( const VkCommandBuffer cmd ) const {
        struct PushConstants {
            ImageId Input;
            ImageId Output;
            i32     Mipmap;
            f32     Roughness;
        } pc = {
                .Input     = m_skybox,
                .Output    = m_radiance,
                .Mipmap    = 0,
                .Roughness = 0.0f };

        static auto pipeline_config = PipelineConfig{
                .name                 = "radiance",
                .compute              = "../shaders/radiance.comp.spv",
                .pushConstantRanges   = { { .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                                            .offset     = 0,
                                            .size       = sizeof( PushConstants ) } },
                .descriptorSetLayouts = { vkctx->GetBindlessLayout( ) } };
        const auto pipeline = vkctx->GetOrCreatePipeline( pipeline_config );

        const auto& output = vkctx->ImageCodex.GetImage( m_radiance );
        output.TransitionLayout( cmd, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL );

        vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->GetVkResource( ) );

        const auto bindless_set = vkctx->GetBindlessSet( );
        vkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->GetLayout( ), 0, 1, &bindless_set, 0, nullptr );

        for ( auto mip = 0; mip < 6; mip++ ) {
            const float roughness = static_cast<float>( mip ) / static_cast<float>( 6 - 1 );

            pc.Mipmap    = mip;
            pc.Roughness = roughness;

            vkCmdPushConstants( cmd, pipeline->GetLayout( ), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( PushConstants ), &pc );
            vkCmdDispatch( cmd, ( output.GetExtent( ).width + 15 ) / 16, ( output.GetExtent( ).height + 15 ) / 16, 6 );
        }
    }

    void Ibl::GenerateBrdf( const VkCommandBuffer cmd ) const {
        const struct PushConstants {
            ImageId Output;
        } pc = { .Output = m_brdf };

        static auto pipeline_config = PipelineConfig{
                .name                 = "brdf",
                .compute              = "../shaders/brdf.comp.spv",
                .pushConstantRanges   = { { .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                                            .offset     = 0,
                                            .size       = sizeof( PushConstants ) } },
                .descriptorSetLayouts = { vkctx->GetBindlessLayout( ) } };
        const auto pipeline = vkctx->GetOrCreatePipeline( pipeline_config );

        const auto& output = vkctx->ImageCodex.GetImage( m_brdf );
        output.TransitionLayout( cmd, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL );

        vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->GetVkResource( ) );

        const auto bindless_set = vkctx->GetBindlessSet( );
        vkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->GetLayout( ), 0, 1, &bindless_set, 0, nullptr );

        vkCmdPushConstants( cmd, pipeline->GetLayout( ), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( PushConstants ), &pc );
        vkCmdDispatch( cmd, ( output.GetExtent( ).width + 15 ) / 16, ( output.GetExtent( ).height + 15 ) / 16, 6 );
    }

} // namespace TL
