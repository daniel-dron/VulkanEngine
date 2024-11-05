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

#include "imgui_pipeline.h"

#include <algorithm>
#include <graphics/utils/vk_initializers.h>
#include <graphics/utils/vk_pipelines.h>

#include "../../engine/tl_engine.h"
#include "../resources/r_resources.h"

using namespace vk_init;

static constexpr int MAX_IDX_COUNT = 1000000;
static constexpr int MAX_VTX_COUNT = 1000000;

ImGuiPipeline::Result<> ImGuiPipeline::Init( TL_VkContext& gfx ) {
    // Setup imgui resources in our engine
    auto& io               = ImGui::GetIO( );
    io.BackendRendererName = "Vulkan Bindless Backend";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

    // font
    {
        uint8_t* data   = nullptr;
        int      width  = 0;
        int      height = 0;
        io.Fonts->GetTexDataAsRGBA32( &data, &width, &height );
        m_fontTextureId = ( ImTextureID ) static_cast<uintptr_t>( gfx.ImageCodex.LoadImageFromData(
                "ImGui Font", data,
                VkExtent3D{ .width  = static_cast<uint32_t>( width ),
                            .height = static_cast<uint32_t>( height ),
                            .depth  = 1 },
                VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, false ) );
        io.Fonts->SetTexID( m_fontTextureId );
    }

    // buffers
    {
        m_indexBuffer = std::make_shared<TL::Buffer>( TL::BufferType::TImGuiIndex, sizeof( ImDrawIdx ) * MAX_IDX_COUNT,
                                                      TL_VkContext::FrameOverlap, nullptr, "[TL] ImGui Index Buffer" );

        m_vertexBuffer =
                std::make_shared<TL::Buffer>( TL::BufferType::TImGuiVertex, sizeof( ImDrawVert ) * MAX_VTX_COUNT,
                                              TL_VkContext::FrameOverlap, nullptr, "[TL] ImGui Vertex Buffer" );
    }

    auto& frag_shader = gfx.shaderStorage->Get( "imgui", TFragment );
    auto& vert_shader = gfx.shaderStorage->Get( "imgui", TVertex );

    VkPushConstantRange range = {
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset     = 0,
            .size       = sizeof( PushConstants ),
    };

    auto bindless_layout = gfx.GetBindlessLayout( );

    VkDescriptorSetLayout layouts[] = { bindless_layout };

    // pipeline
    VkPipelineLayoutCreateInfo layout_info = PipelineLayoutCreateInfo( );
    layout_info.pSetLayouts                = layouts;
    layout_info.setLayoutCount             = 1;
    layout_info.pPushConstantRanges        = &range;
    layout_info.pushConstantRangeCount     = 1;
    VKCALL( vkCreatePipelineLayout( gfx.device, &layout_info, nullptr, &m_layout ) );

    PipelineBuilder builder;
    builder.SetShaders( vert_shader.handle, frag_shader.handle );
    builder.SetInputTopology( VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST );
    builder.SetPolygonMode( VK_POLYGON_MODE_FILL );
    builder.SetCullMode( VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE );
    builder.SetMultisamplingNone( );
    builder.EnableBlending( VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                            VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA );
    builder.DisableDepthTest( );

    builder.SetColorAttachmentFormat( gfx.format );
    builder.SetLayout( m_layout );
    m_pipeline = builder.Build( gfx.device );

#ifdef ENABLE_DEBUG_UTILS
    const VkDebugUtilsObjectNameInfoEXT obj = {
            .sType        = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
            .pNext        = nullptr,
            .objectType   = VK_OBJECT_TYPE_PIPELINE,
            .objectHandle = reinterpret_cast<uint64_t>( m_pipeline ),
            .pObjectName  = "ImGui Pipeline",
    };
    vkSetDebugUtilsObjectNameEXT( gfx.device, &obj );
#endif

    return { };
}

void ImGuiPipeline::Cleanup( TL_VkContext& gfx ) {
    vkDestroyPipelineLayout( gfx.device, m_layout, nullptr );
    vkDestroyPipeline( gfx.device, m_pipeline, nullptr );

    m_indexBuffer.reset( );
    m_vertexBuffer.reset( );
}

void ImGuiPipeline::Draw( TL_VkContext& gfx, VkCommandBuffer cmd, ImDrawData* drawData ) {
    assert( drawData );
    if ( drawData->TotalVtxCount == 0 ) {
        return;
    }

    m_indexBuffer->AdvanceFrame( );
    m_vertexBuffer->AdvanceFrame( );

    // ----------
    // copy buffers
    const auto current_frame_index = gfx.frameNumber % TL_VkContext::FrameOverlap;
    size_t     index_offset        = 0;
    size_t     vertex_offset       = 0;
    for ( i32 i = 0; i < drawData->CmdListsCount; i++ ) {
        const auto& cmd_list = *drawData->CmdLists[i];

        // index
        m_indexBuffer->Upload( cmd_list.IdxBuffer.Data, sizeof( ImDrawIdx ) * index_offset,
                               sizeof( ImDrawIdx ) * cmd_list.IdxBuffer.Size );


        // vertex
        m_vertexBuffer->Upload( cmd_list.VtxBuffer.Data, sizeof( ImDrawVert ) * vertex_offset,
                                sizeof( ImDrawVert ) * cmd_list.VtxBuffer.Size );

        index_offset += cmd_list.IdxBuffer.Size;
        vertex_offset += cmd_list.VtxBuffer.Size;
    }

    vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline );

    auto bindless_set = gfx.GetBindlessSet( );
    vkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_layout, 0, 1, &bindless_set, 0, nullptr );

    auto&      target_image = gfx.ImageCodex.GetImage( gfx.GetCurrentFrame( ).hdrColor );
    VkViewport viewport     = {
                .x        = 0,
                .y        = 0,
                .width    = static_cast<float>( target_image.GetExtent( ).width ),
                .height   = static_cast<float>( target_image.GetExtent( ).height ),
                .minDepth = 0.0f,
                .maxDepth = 1.0f,
    };
    vkCmdSetViewport( cmd, 0, 1, &viewport );

    auto clip_offset = drawData->DisplayPos;
    auto clip_scale  = drawData->FramebufferScale;

    i32 global_idx_offset = 0;
    i32 global_vtx_offset = 0;

    vkCmdBindIndexBuffer( cmd, m_indexBuffer->GetVkResource( ), m_indexBuffer->GetCurrentOffset( ),
                          VK_INDEX_TYPE_UINT16 );

    for ( int cmd_list_id = 0; cmd_list_id < drawData->CmdListsCount; cmd_list_id++ ) {
        const auto& cmd_list = *drawData->CmdLists[cmd_list_id];
        for ( int cmd_id = 0; cmd_id < cmd_list.CmdBuffer.Size; cmd_id++ ) {
            const auto& im_cmd = cmd_list.CmdBuffer[cmd_id];
            if ( im_cmd.UserCallback && im_cmd.UserCallback != ImDrawCallback_ResetRenderState ) {
                im_cmd.UserCallback( &cmd_list, &im_cmd );
                continue;
            }

            if ( im_cmd.ElemCount == 0 ) {
                continue;
            }

            auto clip_min = ImVec2( ( im_cmd.ClipRect.x - clip_offset.x ) * clip_scale.x,
                                    ( im_cmd.ClipRect.y - clip_offset.y ) * clip_scale.y );
            auto clip_max = ImVec2( ( im_cmd.ClipRect.z - clip_offset.x ) * clip_scale.x,
                                    ( im_cmd.ClipRect.w - clip_offset.y ) * clip_scale.y );
            clip_min.x    = std::clamp( clip_min.x, 0.0f, viewport.width );
            clip_max.x    = std::clamp( clip_max.x, 0.0f, viewport.width );
            clip_min.y    = std::clamp( clip_min.y, 0.0f, viewport.height );
            clip_max.y    = std::clamp( clip_max.y, 0.0f, viewport.height );
            if ( clip_max.x <= clip_min.x || clip_max.y <= clip_min.y ) {
                continue;
            }

            auto texture_id = gfx.ImageCodex.GetWhiteImageId( );
            if ( im_cmd.TextureId != 0 ) {
                texture_id = static_cast<ImageId>( reinterpret_cast<uint64_t>( im_cmd.TextureId ) );
            }

            bool        is_srgb = true;
            const auto& texture = gfx.ImageCodex.GetImage( texture_id );
            if ( texture.GetFormat( ) == VK_FORMAT_R8G8B8A8_SRGB ||
                 texture.GetFormat( ) == VK_FORMAT_R16G16B16A16_SFLOAT ) {
                is_srgb = false;
            }

            const auto scale = glm::vec2( 2.0f / drawData->DisplaySize.x, 2.0f / drawData->DisplaySize.y );
            const auto translate =
                    glm::vec2( -1.0f - drawData->DisplayPos.x * scale.x, -1.0f - drawData->DisplayPos.y * scale.y );

            // set scissor
            const auto scissor_x = static_cast<std::int32_t>( clip_min.x );
            const auto scissor_y = static_cast<std::int32_t>( clip_min.y );
            const auto s_width   = static_cast<std::uint32_t>( clip_max.x - clip_min.x );
            const auto s_height  = static_cast<std::uint32_t>( clip_max.y - clip_min.y );
            const auto scissor   = VkRect2D{
                      .offset = { scissor_x, scissor_y },
                      .extent = { s_width, s_height },
            };
            vkCmdSetScissor( cmd, 0, 1, &scissor );

            PushConstants pc = {
                    .vertexBuffer = m_vertexBuffer->GetDeviceAddress( ),
                    .textureId    = ( uint32_t )texture_id,
                    .isSrgb       = is_srgb,
                    .offset       = translate,
                    .scale        = scale,
            };

            vkCmdPushConstants( cmd, m_layout, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT, 0,
                                sizeof( PushConstants ), &pc );
            vkCmdDrawIndexed( cmd, im_cmd.ElemCount, 1, im_cmd.IdxOffset + global_idx_offset,
                              im_cmd.VtxOffset + im_cmd.VtxOffset + global_vtx_offset, 0 );
        }

        global_idx_offset += cmd_list.IdxBuffer.Size;
        global_vtx_offset += cmd_list.VtxBuffer.Size;
    }
}
