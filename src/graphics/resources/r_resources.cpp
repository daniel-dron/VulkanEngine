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

#include "r_resources.h"

#include <SDL2/SDL_vulkan.h>
#include <VkBootstrap.h>
#include <graphics/utils/vk_initializers.h>
#include <imgui.h>

#include "r_pipeline.h"

using namespace vkb;

namespace TL {
    std::unique_ptr<TL_VkContext> vkctx = nullptr;
}

constexpr bool B_USE_VALIDATION_LAYERS = true;

ImmediateExecutor::Result<> ImmediateExecutor::Init( TL_VkContext* gfx ) {
    this->m_gfx = gfx;

    // Fence creation
    constexpr VkFenceCreateInfo fence_info = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };
    VKCALL( vkCreateFence( gfx->device, &fence_info, nullptr, &fence ) );

    // Pool and buffer creation
    const VkCommandPoolCreateInfo pool_info = { .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                                .pNext            = nullptr,
                                                .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                                .queueFamilyIndex = gfx->graphicsQueueFamily };
    VKCALL( vkCreateCommandPool( gfx->device, &pool_info, nullptr, &pool ) );

    const VkCommandBufferAllocateInfo buffer_info = { .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                                      .pNext              = nullptr,
                                                      .commandPool        = pool,
                                                      .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                                      .commandBufferCount = 1 };
    VKCALL( vkAllocateCommandBuffers( gfx->device, &buffer_info, &commandBuffer ) );

    return { };
}

void ImmediateExecutor::Execute( std::function<void( VkCommandBuffer cmd )>&& func ) {
    mutex.lock( );

    VKCALL( vkResetFences( m_gfx->device, 1, &fence ) );
    VKCALL( vkResetCommandBuffer( commandBuffer, 0 ) );

    const auto                         cmd  = commandBuffer;
    constexpr VkCommandBufferBeginInfo info = {
            .sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext            = nullptr,
            .flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            .pInheritanceInfo = nullptr,
    };
    VKCALL( vkBeginCommandBuffer( cmd, &info ) );

    func( cmd );

    VKCALL( vkEndCommandBuffer( cmd ) );

    const VkCommandBufferSubmitInfo submit_info = {
            .sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
            .pNext         = nullptr,
            .commandBuffer = cmd,
            .deviceMask    = 0,
    };
    const VkSubmitInfo2 submit = {
            .sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
            .pNext                    = nullptr,
            .waitSemaphoreInfoCount   = 0,
            .pWaitSemaphoreInfos      = nullptr,
            .commandBufferInfoCount   = 1,
            .pCommandBufferInfos      = &submit_info,
            .signalSemaphoreInfoCount = 0,
            .pSignalSemaphoreInfos    = nullptr,
    };

    VKCALL( vkQueueSubmit2( m_gfx->graphicsQueue, 1, &submit, fence ) );
    VKCALL( vkWaitForFences( m_gfx->device, 1, &fence, true, 9999999999 ) );

    mutex.unlock( );
}

void ImmediateExecutor::Cleanup( ) const {
    vkDestroyFence( m_gfx->device, fence, nullptr );
    vkDestroyCommandPool( m_gfx->device, pool, nullptr );
}

//==============================================================================//
//                             Materials                                        //
//==============================================================================//

void TL::renderer::MaterialPool::Init( ) {
    // Fill free list backwards
    m_freeIndices.reserve( MAX_MATERIALS );
    for ( uint16_t i = 0; i < MAX_MATERIALS; i++ ) {
        m_freeIndices.push_back( MAX_MATERIALS - 1 - i );
    }

    // Initialize generations to 0
    m_generations.fill( 0 );

    m_materialsGpuBuffer = std::make_unique<Buffer>( BufferType::TStorage, MAX_MATERIALS * sizeof( MaterialData ), 1, nullptr, "[TL] Material Data" );
}

void TL::renderer::MaterialPool::Shutdown( ) {
    // Implictly call the buffer destructor
    m_materialsGpuBuffer.reset( );
}

TL::renderer::MaterialHandle TL::renderer::MaterialPool::CreateMaterial( const Material& material ) {
    MaterialData material_data = { };

    // Fill gpu data
    material_data.BaseColor     = material.BaseColor;
    material_data.EmissiveColor = Vec4( 0.0f, 0.0f, 0.0f, 0.0f ); // TODO: add emissiveness

    material_data.TextureIndices[0] = material.ColorId;
    material_data.TextureIndices[1] = material.MetalRoughnessId;
    material_data.TextureIndices[2] = material.NormalId;
    material_data.TextureIndices[3] = 0; // Unused

    material_data.Factors[0] = material.MetalnessFactor;
    material_data.Factors[1] = material.roughnessFactor;
    material_data.Factors[2] = 0; // Unused
    material_data.Factors[3] = 0; // Unused

    // Get available index and save data
    const u16 index = m_freeIndices.back( );
    m_freeIndices.pop_back( );
    m_materialDatas[index] = material_data;

    // Generation increment only happens on destruction to invalidate current ones

    // Upload data to the gpu
    m_materialsGpuBuffer->Upload( &material_data, index * sizeof( MaterialData ), sizeof( MaterialData ) );

    return MaterialHandle{ index, m_generations[index] };
}

void TL::renderer::MaterialPool::DestroyMaterial( const MaterialHandle handle ) {
    if ( IsValid( handle ) ) {
        // Increase generation to invalidate existing handles
        m_generations[handle.index]++;

        // Register index as available
        m_freeIndices.push_back( handle.index );
    }
}

TL::renderer::MaterialData& TL::renderer::MaterialPool::GetMaterial( const MaterialHandle handle ) {
    assert( IsValid( handle ) );
    return m_materialDatas[handle.index];
}

std::optional<TL::renderer::MaterialData*> TL::renderer::MaterialPool::GetMaterialSafe( MaterialHandle handle ) {
    if ( IsValid( handle ) ) {
        return &m_materialDatas[handle.index];
    }

    return std::nullopt;
}

void TL::renderer::MaterialPool::UpdateMaterial( const MaterialHandle handle ) {
    if ( IsValid( handle ) ) {
        m_materialsGpuBuffer->Upload( &m_materialDatas[handle.index], handle.index * sizeof( MaterialData ), sizeof( MaterialData ) );
    }
}

bool TL::renderer::MaterialPool::IsValid( MaterialHandle handle ) const {
    if ( handle.index > MAX_MATERIALS ) {
        return false;
    }

    return m_generations[handle.index] == handle.generation;
}

//==============================================================================//
//==============================================================================//

//==============================================================================//
//                               Meshes                                         //
//==============================================================================//

void TL::renderer::MeshPool::Init( ) {
    // Fill free list backwards
    m_freeIndices.reserve( MAX_MESHES );
    for ( uint16_t i = 0; i < MAX_MESHES; i++ ) {
        m_freeIndices.push_back( MAX_MESHES - 1 - i );
    }

    // Initialize generations to 0
    m_generations.fill( 0 );
}

void TL::renderer::MeshPool::Shutdown( ) {
    for ( auto& mesh : m_meshDatas ) {
        // Guaranteed safe call to reset by the standard even if they have not been allocated (no-op in that case)
        mesh.IndexBuffer.reset( );
        mesh.VertexBuffer.reset( );
    }
}
TL::renderer::MeshHandle TL::renderer::MeshPool::CreateMesh( const MeshContent& content ) {
    // Get available index and get reference to MeshData
    const u16 index = m_freeIndices.back( );
    m_freeIndices.pop_back( );
    auto& mesh_data = m_meshDatas[index];

    // Create handle
    MeshHandle handle = { index, m_generations[index] };

    mesh_data.Aabb = content.Aabb;

    // Calculate buffer sizes
    const size_t vertex_buffer_size = content.Vertices.size( ) * sizeof( Vertex );
    const size_t index_buffer_size  = content.Indices.size( ) * sizeof( u32 );

    std::string name_vertex = std::format( "{}{}.(vtx)", index, handle.generation );
    std::string name_index  = std::format( "{}{}.(idx)", index, handle.generation );

    // Staging buffer to copy to the buffers.
    // The vertex and index buffers are GPU only for performance reasons.
    mesh_data.VertexBuffer = std::make_unique<Buffer>( BufferType::TVertex, vertex_buffer_size, 1, nullptr, name_vertex.c_str( ) );
    mesh_data.IndexBuffer  = std::make_unique<Buffer>( BufferType::TIndex, index_buffer_size, 1, nullptr, name_index.c_str( ) );
    const auto staging     = Buffer( BufferType::TStaging, vertex_buffer_size + index_buffer_size, 1, nullptr, "Staging" );

    staging.Upload( content.Vertices.data( ), vertex_buffer_size );
    staging.Upload( content.Indices.data( ), vertex_buffer_size, index_buffer_size );

    vkctx->Execute( [&]( const VkCommandBuffer cmd ) {
        const VkBufferCopy vertex_copy = {
                .srcOffset = 0,
                .dstOffset = 0,
                .size      = vertex_buffer_size };
        vkCmdCopyBuffer( cmd, staging.GetVkResource( ), mesh_data.VertexBuffer->GetVkResource( ), 1, &vertex_copy );

        const VkBufferCopy index_copy = {
                .srcOffset = vertex_buffer_size, .dstOffset = 0, .size = index_buffer_size };
        vkCmdCopyBuffer( cmd, staging.GetVkResource( ), mesh_data.IndexBuffer->GetVkResource( ), 1, &index_copy );
    } );

    mesh_data.IndexCount = static_cast<u32>( content.Indices.size( ) );
    mesh_data.Content    = content;

    return handle;
}

void TL::renderer::MeshPool::DestroyMesh( const MeshHandle handle ) {
    if ( IsValid( handle ) ) {
        // Free buffers as they are no longer needed
        auto& mesh_data = m_meshDatas[handle.index];
        mesh_data.VertexBuffer.reset( );
        mesh_data.IndexBuffer.reset( );

        // Increase generation to invalidate existing handles
        m_generations[handle.index]++;

        // Register index as available
        m_freeIndices.push_back( handle.index );
    }
}

TL::renderer::MeshData& TL::renderer::MeshPool::GetMesh( const MeshHandle handle ) {
    return m_meshDatas[handle.index];
}

std::optional<TL::renderer::MeshData*> TL::renderer::MeshPool::GetMeshSafe( MeshHandle handle ) {
    if ( IsValid( handle ) ) {
        return &m_meshDatas[handle.index];
    }

    return std::nullopt;
}

bool TL::renderer::MeshPool::IsValid( MeshHandle handle ) const {
    if ( handle.index > MAX_MESHES ) {
        return false;
    }

    return m_generations[handle.index] == handle.generation;
}


//==============================================================================//
//==============================================================================//

//==============================================================================//
//                             VK CONTEXT                                       //
//==============================================================================//

TL_VkContext::TL_VkContext( SDL_Window* window ) {
    InitDevice( window );
    InitAllocator( );
}

TL_VkContext::Result<> TL_VkContext::Init( ) {

    shaderStorage = std::make_unique<ShaderStorage>( this );

    // descriptor pool
    std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> ratios = {
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1.0f },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1.0f },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1.0f },
    };
    setPool.Init( device, 100, ratios );

    executor.Init( this );

    // Resource
    MaterialPool.Init( );
    MeshPool.Init( );

    ImageCodex.Init( this );

    InitSwapchain( WIDTH, HEIGHT );

    return { };
}

void TL_VkContext::RecreateSwapchain( u32 width, u32 height ) { InitSwapchain( width, height ); }

void TL_VkContext::Execute( std::function<void( VkCommandBuffer )>&& func ) { executor.Execute( std::move( func ) ); }

void TL_VkContext::Cleanup( ) {
    CleanupSwapchain( );

    m_pipelines.clear( );

    MeshPool.Shutdown( );
    MaterialPool.Shutdown( );

    // materialCodex.Cleanup( );
    ImageCodex.Cleanup( );
    executor.Cleanup( );
    setPool.DestroyPools( device );
    shaderStorage->Cleanup( );
    vkFreeCommandBuffers( device, computeCommandPool, 1, &computeCommand );
    vkDestroyCommandPool( device, computeCommandPool, nullptr );
    vmaDestroyAllocator( allocator );
    vkDestroySurfaceKHR( instance, surface, nullptr );
    vkDestroyDevice( device, nullptr );
    destroy_debug_utils_messenger( instance, debugMessenger );
    vkDestroyInstance( instance, nullptr );
}

std::shared_ptr<TL::Pipeline> TL_VkContext::GetOrCreatePipeline( const TL::PipelineConfig& config ) {
    assert( config.name != nullptr );

    // Check if pipeline already exists
    if ( m_pipelines.find( config.name ) != m_pipelines.end( ) ) {
        return m_pipelines[config.name];
    }

    m_pipelines[config.name] = std::make_shared<TL::Pipeline>( config );

    return m_pipelines[config.name];
}

VkDescriptorSet TL_VkContext::AllocateSet( VkDescriptorSetLayout layout ) { return setPool.Allocate( device, layout ); }

MultiDescriptorSet TL_VkContext::AllocateMultiSet( VkDescriptorSetLayout layout ) {
    std::vector<VkDescriptorSet> sets;
    for ( auto i = 0; i < FrameOverlap; i++ ) {
        sets.push_back( AllocateSet( layout ) );
    }

    MultiDescriptorSet md_set;
    md_set.m_sets = sets;

    return md_set;
}

VkDescriptorSetLayout TL_VkContext::GetBindlessLayout( ) const { return ImageCodex.GetBindlessLayout( ); }

VkDescriptorSet TL_VkContext::GetBindlessSet( ) const { return ImageCodex.GetBindlessSet( ); }

float TL_VkContext::GetTimestampInMs( uint64_t start, uint64_t end ) const {
    const auto period = deviceProperties.properties.limits.timestampPeriod;
    return ( end - start ) * period / 1000000.0f;
}

void TL_VkContext::SetObjectDebugName( VkObjectType type, void* object, const std::string& name ) {
#ifdef ENABLE_DEBUG_FEATURES
    const VkDebugUtilsObjectNameInfoEXT obj = {
            .sType        = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
            .pNext        = nullptr,
            .objectType   = type,
            .objectHandle = reinterpret_cast<uint64_t>( object ),
            .pObjectName  = name.c_str( ),
    };
    vkSetDebugUtilsObjectNameEXT( device, &obj );
#endif
}

TL_FrameData& TL_VkContext::GetCurrentFrame( ) { return frames.at( frameNumber % FrameOverlap ); }

void TL_VkContext::DrawDebug( ) const {
    auto       props  = deviceProperties.properties;
    const auto limits = props.limits;

    ImGui::Text( "Device Name: %s", props.deviceName );
    ImGui::Text( "Driver Version: %d.%d.%d", VK_VERSION_MAJOR( props.driverVersion ),
                 VK_VERSION_MINOR( props.driverVersion ), VK_VERSION_PATCH( props.driverVersion ) );

    VkDeviceSize total_vram       = 0;
    VkDeviceSize total_system_ram = 0;
    for ( uint32_t i = 0; i < memProperties.memoryHeapCount; i++ ) {
        if ( memProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT ) {
            total_vram += memProperties.memoryHeaps[i].size;
        }
        else {
            total_system_ram += memProperties.memoryHeaps[i].size;
        }
    }
    ImGui::Text( "Total VRAM: %.2f GB", total_vram / ( 1024.0 * 1024.0 * 1024.0 ) );
    ImGui::Text( "Total System RAM: %.2f GB", total_system_ram / ( 1024.0 * 1024.0 * 1024.0 ) );

    ImGui::Text( "Max Uniform Buffer Range: %zu bytes", limits.maxUniformBufferRange );
    ImGui::Text( "Max Storage Buffer Range: %zu bytes", limits.maxStorageBufferRange );
    ImGui::Text( "Max Push Constants Size: %zu bytes", limits.maxPushConstantsSize );

    ImGui::Text( "Max Compute Shared Memory Size: %zu bytes", limits.maxComputeSharedMemorySize );
    ImGui::Text( "Max Compute Work Group Count: %d x %d x %d", limits.maxComputeWorkGroupCount[0],
                 limits.maxComputeWorkGroupCount[1], limits.maxComputeWorkGroupCount[2] );
    ImGui::Text( "Max Compute Work Group Invocations: %d", limits.maxComputeWorkGroupInvocations );

    ImGui::Text( "Max Framebuffer Width: %d", limits.maxFramebufferWidth );
    ImGui::Text( "Max Framebuffer Height: %d", limits.maxFramebufferHeight );
    ImGui::Text( "Max Image Dimension 2D: %d", limits.maxImageDimension2D );
    ImGui::Text( "Max Image Array Layers: %d", limits.maxImageArrayLayers );

    ImGui::Text( "Geometry Shader Support: %s", ( props.limits.maxGeometryShaderInvocations > 0 ) ? "Yes" : "No" );
    ImGui::Text( "Tessellation Shader Support: %s",
                 ( props.limits.maxTessellationGenerationLevel > 0 ) ? "Yes" : "No" );
}

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback( VkDebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,
                                              VkDebugUtilsMessageTypeFlagsEXT             messageType,
                                              const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                              void*                                       pUserData ) {

    auto& engine = TL_Engine::Get( );
    engine.console.AddLog( "{}", pCallbackData->pMessage );

    return VK_FALSE;
}

TL_VkContext::Result<> TL_VkContext::InitDevice( SDL_Window* window ) {
    // Instance
    InstanceBuilder builder;
    auto            instance_prototype = builder.set_app_name( "Vulkan Engine" )
                                      .request_validation_layers( B_USE_VALIDATION_LAYERS )
                                      .enable_extension( "VK_EXT_debug_utils" )
                                      .set_debug_callback( debugCallback )
                                      .require_api_version( 1, 3, 0 )
                                      .build( );

    if ( !instance_prototype.has_value( ) ) {
        return std::unexpected( GfxDeviceError{ Error::InstanceCreationFailed } );
    }

    auto& bs_instance = instance_prototype.value( );

    instance = bs_instance.instance;

    // Surface
    SDL_Vulkan_CreateSurface( window, instance, &surface );

    // Device
    VkPhysicalDeviceVulkan13Features features_13{
            .sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
            .synchronization2 = true,
            .dynamicRendering = true,
    };
    VkPhysicalDeviceVulkan12Features features_12{
            .sType                                        = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
            .descriptorIndexing                           = true,
            .descriptorBindingSampledImageUpdateAfterBind = true,
            .descriptorBindingStorageImageUpdateAfterBind = true,
            .descriptorBindingPartiallyBound              = true,
            .runtimeDescriptorArray                       = true,
            .scalarBlockLayout                            = true,
            .hostQueryReset                               = true,
            .bufferDeviceAddress                          = true,
    };
    VkPhysicalDeviceVulkan11Features features_11{
            .sType                         = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
            .multiview                     = true,
            .variablePointersStorageBuffer = true,
            .variablePointers              = true,
            .shaderDrawParameters          = true,
    };
    VkPhysicalDeviceFeatures features{
            .multiDrawIndirect = true,
            .fillModeNonSolid  = true,
            .samplerAnisotropy = true,
            .shaderInt64       = true,
    };

    PhysicalDeviceSelector selector{ bs_instance };
    auto                   physical_device_res = selector.set_minimum_version( 1, 3 )
                                       .set_required_features_13( features_13 )
                                       .set_required_features_12( features_12 )
                                       .set_required_features_11( features_11 )
                                       .set_required_features( features )
                                       .add_required_extension( "VK_EXT_descriptor_indexing" )
                                       .add_required_extension( "VK_KHR_synchronization2" )
                                       .add_required_extension( "VK_KHR_multiview" )
                                       .add_required_extension( "VK_EXT_host_query_reset" )
                                       .set_surface( surface )
                                       .select( );
    if ( !physical_device_res ) {
        return std::unexpected( GfxDeviceError{ Error::PhysicalDeviceSelectionFailed } );
    }

    auto& physical_device = physical_device_res.value( );
    chosenGpu             = physical_device;

    for ( const auto& ext : physical_device.get_available_extensions( ) ) {
        TL_Engine::Get( ).console.AddLog( "{}", ext.c_str( ) );
    }

    deviceProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    vkGetPhysicalDeviceProperties2( physical_device, &deviceProperties );

    assert( deviceProperties.properties.limits.timestampPeriod != 0 &&
            "Timestamp queries not supported on this device!" );

    vkGetPhysicalDeviceMemoryProperties( chosenGpu, &memProperties );

    // Logical Device
    DeviceBuilder device_builder{ physical_device };
    auto          device_res = device_builder.build( );
    if ( !device_res ) {
        return std::unexpected( GfxDeviceError{ Error::LogicalDeviceCreationFailed } );
    }

    auto& bs_device = device_res.value( );
    device          = bs_device;

    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties( physical_device.physical_device, &properties );

    // Access maxDescriptorSetCount
    uint32_t max_descriptor_set_count = properties.limits.maxDescriptorSetSampledImages;
    TL_Engine::Get( ).console.AddLog( "Max sampled images: {}", max_descriptor_set_count );

    // Queue
    graphicsQueue       = bs_device.get_queue( QueueType::graphics ).value( );
    graphicsQueueFamily = bs_device.get_queue_index( QueueType::graphics ).value( );

    computeQueue       = bs_device.get_queue( QueueType::compute ).value( );
    computeQueueFamily = bs_device.get_queue_index( QueueType::compute ).value( );
    const VkCommandPoolCreateInfo command_pool_info =
            vk_init::CommandPoolCreateInfo( computeQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT );
    VKCALL( vkCreateCommandPool( device, &command_pool_info, nullptr, &computeCommandPool ) );
    VkCommandBufferAllocateInfo cmd_alloc_info = vk_init::CommandBufferAllocateInfo( computeCommandPool, 1 );
    VKCALL( vkAllocateCommandBuffers( device, &cmd_alloc_info, &computeCommand ) );

    InitDebugFunctions( );

    return { };
}

TL_VkContext::Result<> TL_VkContext::InitAllocator( ) {
    const VmaAllocatorCreateInfo info = {
            .flags          = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
            .physicalDevice = chosenGpu,
            .device         = device,
            .instance       = instance,
    };

    if ( vmaCreateAllocator( &info, &allocator ) != VK_SUCCESS ) {
        return std::unexpected( GfxDeviceError{ Error::GlobalAllocatorFailed } );
    }

    return { };
}

void TL_VkContext::InitSwapchain( const u32 width, const u32 height ) {
    extent = VkExtent2D( width, height );

    SwapchainBuilder builder( chosenGpu, device, surface );
    format = VK_FORMAT_R8G8B8A8_SRGB;

    auto old_swapchain = swapchain;

    auto swapchain_res = builder.set_desired_format( VkSurfaceFormatKHR{
                                                             .format     = format,
                                                             .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
                                                     } )
                                 .set_desired_present_mode( presentMode )
                                 .set_old_swapchain( old_swapchain )
                                 .add_image_usage_flags( VK_IMAGE_USAGE_TRANSFER_DST_BIT )
                                 .build( );

    // TODO: throw instead
    assert( swapchain_res.has_value( ) );
    auto& bs_swapchain = swapchain_res.value( );

    if ( old_swapchain != VK_NULL_HANDLE ) {
        CleanupSwapchain( );
    }

    swapchain = bs_swapchain.swapchain;
    images    = bs_swapchain.get_images( ).value( );
    views     = bs_swapchain.get_image_views( ).value( );

    assert( images.size( ) != 0 );
    assert( views.size( ) != 0 );

    const VkCommandPoolCreateInfo command_pool_info =
            vk_init::CommandPoolCreateInfo( graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT );
    for ( int i = 0; i < FrameOverlap; i++ ) {
        VKCALL( vkCreateCommandPool( device, &command_pool_info, nullptr, &frames[i].pool ) );

        VkCommandBufferAllocateInfo cmd_alloc_info = vk_init::CommandBufferAllocateInfo( frames[i].pool, 1 );
        VKCALL( vkAllocateCommandBuffers( device, &cmd_alloc_info, &frames[i].commandBuffer ) );

        const VkDebugUtilsObjectNameInfoEXT obj = {
                .sType        = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
                .pNext        = nullptr,
                .objectType   = VK_OBJECT_TYPE_COMMAND_BUFFER,
                .objectHandle = reinterpret_cast<uint64_t>( frames[i].commandBuffer ),
                .pObjectName  = "Main CMD",
        };
        vkSetDebugUtilsObjectNameEXT( device, &obj );
    }

    auto fenceCreateInfo     = vk_init::FenceCreateInfo( VK_FENCE_CREATE_SIGNALED_BIT );
    auto semaphoreCreateInfo = vk_init::SemaphoreCreateInfo( );

    for ( int i = 0; i < FrameOverlap; i++ ) {
        VKCALL( vkCreateFence( device, &fenceCreateInfo, nullptr, &frames[i].fence ) );

        VKCALL( vkCreateSemaphore( device, &semaphoreCreateInfo, nullptr, &frames[i].renderSemaphore ) );
        VKCALL( vkCreateSemaphore( device, &semaphoreCreateInfo, nullptr, &frames[i].swapchainSemaphore ) );
    }

    const VkExtent3D draw_image_extent = { .width = extent.width, .height = extent.height, .depth = 1 };

    VkImageUsageFlags draw_image_usages{ };
    draw_image_usages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    draw_image_usages |= VK_IMAGE_USAGE_SAMPLED_BIT;

    VkImageUsageFlags depth_image_usages{ };
    depth_image_usages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    depth_image_usages |= VK_IMAGE_USAGE_SAMPLED_BIT;

    constexpr VkImageUsageFlags usages = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    const VkExtent3D            extent = { .width = this->extent.width, .height = this->extent.height, .depth = 1 };

    // TODO: transition to correct layout ( check validation layers )
    for ( auto& frame : frames ) {
        std::vector<unsigned char> empty_image_data;
        empty_image_data.resize( extent.width * extent.height * 8, 0 );
        frame.hdrColor = ImageCodex.LoadImageFromData( "hdr image pbr", empty_image_data.data( ), draw_image_extent,
                                                       VK_FORMAT_R16G16B16A16_SFLOAT, draw_image_usages, false );

        frame.postProcessImage =
                ImageCodex.CreateEmptyImage( "post process", draw_image_extent, VK_FORMAT_R8G8B8A8_UNORM,
                                             draw_image_usages | VK_IMAGE_USAGE_STORAGE_BIT, false );
        auto& ppi = ImageCodex.GetImage( frame.postProcessImage );
        ImageCodex.bindlessRegistry.AddStorageImage( *this, frame.postProcessImage, ppi.GetBaseView( ) );

        empty_image_data.resize( extent.width * extent.height * 4, 0 );
        frame.depth = ImageCodex.LoadImageFromData( "main depth image", empty_image_data.data( ), draw_image_extent,
                                                    VK_FORMAT_D32_SFLOAT, depth_image_usages, false );
        std::vector<unsigned char> empty_data;
        empty_data.resize( extent.width * extent.height * 4 * 2, 0 );

        frame.gBuffer.position = ImageCodex.LoadImageFromData( "gbuffer.position", empty_data.data( ), extent,
                                                               VK_FORMAT_R16G16B16A16_SFLOAT, usages, false );
        frame.gBuffer.normal   = ImageCodex.LoadImageFromData( "gbuffer.normal", empty_data.data( ), extent,
                                                               VK_FORMAT_R16G16B16A16_SFLOAT, usages, false );
        frame.gBuffer.pbr      = ImageCodex.LoadImageFromData( "gbuffer.pbr", empty_data.data( ), extent,
                                                               VK_FORMAT_R16G16B16A16_SFLOAT, usages, false );
        frame.gBuffer.albedo   = ImageCodex.LoadImageFromData( "gbuffer.albedo", empty_data.data( ), extent,
                                                               VK_FORMAT_R16G16B16A16_SFLOAT, usages, false );

        // Query Pool
        VkQueryPoolCreateInfo query_pool_create_info = {
                .sType      = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
                .pNext      = nullptr,
                .queryType  = VK_QUERY_TYPE_TIMESTAMP,
                .queryCount = static_cast<uint32_t>( frame.gpuTimestamps.size( ) ),
        };
        VKCALL( vkCreateQueryPool( device, &query_pool_create_info, nullptr, &frame.queryPoolTimestamps ) );
    }
}
void TL_VkContext::CleanupSwapchain( ) {

    for ( uint64_t i = 0; i < FrameOverlap; i++ ) {
        auto& frame = frames[i];

        auto oldDepth    = frame.depth;
        auto oldHdr      = frame.hdrColor;
        auto oldPPI      = frame.postProcessImage;
        auto oldAlbedo   = frame.gBuffer.albedo;
        auto oldNormal   = frame.gBuffer.normal;
        auto oldPbr      = frame.gBuffer.pbr;
        auto oldPosition = frame.gBuffer.position;

        frame.deletionQueue.PushFunction( [&, oldDepth, oldHdr, oldPPI, oldAlbedo, oldNormal, oldPbr, oldPosition] {
            // destroy the images
            ImageCodex.UnloadIamge( oldDepth );
            ImageCodex.UnloadIamge( oldHdr );
            ImageCodex.UnloadIamge( oldPPI );
            ImageCodex.UnloadIamge( oldAlbedo );
            ImageCodex.UnloadIamge( oldNormal );
            ImageCodex.UnloadIamge( oldPbr );
            ImageCodex.UnloadIamge( oldPosition );
        } );

        vkDestroyCommandPool( device, frames[i].pool, nullptr );

        // sync objects
        vkDestroyFence( device, frames[i].fence, nullptr );
        vkDestroySemaphore( device, frames[i].renderSemaphore, nullptr );
        vkDestroySemaphore( device, frames[i].swapchainSemaphore, nullptr );

        vkDestroyQueryPool( device, frames[i].queryPoolTimestamps, nullptr );
    }

    vkDestroySwapchainKHR( device, swapchain, nullptr );

    for ( const auto& view : views ) {
        vkDestroyImageView( device, view, nullptr );
    }
}

void TL_VkContext::InitDebugFunctions( ) const {
    // ----------
    // debug utils functions
    Debug::vkCmdBeginDebugUtilsLabelEXT_ptr =
            ( PFN_vkCmdBeginDebugUtilsLabelEXT )vkGetInstanceProcAddr( instance, "vkCmdBeginDebugUtilsLabelEXT" );
    Debug::vkCmdEndDebugUtilsLabelEXT_ptr =
            ( PFN_vkCmdEndDebugUtilsLabelEXT )vkGetInstanceProcAddr( instance, "vkCmdEndDebugUtilsLabelEXT" );
    Debug::vkCmdInsertDebugUtilsLabelEXT_ptr =
            ( PFN_vkCmdInsertDebugUtilsLabelEXT )vkGetInstanceProcAddr( instance, "vkCmdInsertDebugUtilsLabelEXT" );
    Debug::vkCreateDebugUtilsMessengerEXT_ptr =
            ( PFN_vkCreateDebugUtilsMessengerEXT )vkGetInstanceProcAddr( instance, "vkCreateDebugUtilsMessengerEXT" );
    Debug::vkDestroyDebugUtilsMessengerEXT_ptr =
            ( PFN_vkDestroyDebugUtilsMessengerEXT )vkGetInstanceProcAddr( instance, "vkDestroyDebugUtilsMessengerEXT" );
    Debug::vkQueueBeginDebugUtilsLabelEXT_ptr =
            ( PFN_vkQueueBeginDebugUtilsLabelEXT )vkGetInstanceProcAddr( instance, "vkQueueBeginDebugUtilsLabelEXT" );
    Debug::vkQueueEndDebugUtilsLabelEXT_ptr =
            ( PFN_vkQueueEndDebugUtilsLabelEXT )vkGetInstanceProcAddr( instance, "vkQueueEndDebugUtilsLabelEXT" );
    Debug::vkQueueInsertDebugUtilsLabelEXT_ptr =
            ( PFN_vkQueueInsertDebugUtilsLabelEXT )vkGetInstanceProcAddr( instance, "vkQueueInsertDebugUtilsLabelEXT" );
    Debug::vkSetDebugUtilsObjectNameEXT_ptr =
            ( PFN_vkSetDebugUtilsObjectNameEXT )vkGetInstanceProcAddr( instance, "vkSetDebugUtilsObjectNameEXT" );
    Debug::vkSetDebugUtilsObjectTagEXT_ptr =
            ( PFN_vkSetDebugUtilsObjectTagEXT )vkGetInstanceProcAddr( instance, "vkSetDebugUtilsObjectTagEXT" );
    Debug::vkSubmitDebugUtilsMessageEXT_ptr =
            ( PFN_vkSubmitDebugUtilsMessageEXT )vkGetInstanceProcAddr( instance, "vkSubmitDebugUtilsMessageEXT" );
}

namespace Debug {
    PFN_vkCmdBeginDebugUtilsLabelEXT    vkCmdBeginDebugUtilsLabelEXT_ptr    = nullptr;
    PFN_vkCmdEndDebugUtilsLabelEXT      vkCmdEndDebugUtilsLabelEXT_ptr      = nullptr;
    PFN_vkCmdInsertDebugUtilsLabelEXT   vkCmdInsertDebugUtilsLabelEXT_ptr   = nullptr;
    PFN_vkCreateDebugUtilsMessengerEXT  vkCreateDebugUtilsMessengerEXT_ptr  = nullptr;
    PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT_ptr = nullptr;
    PFN_vkQueueBeginDebugUtilsLabelEXT  vkQueueBeginDebugUtilsLabelEXT_ptr  = nullptr;
    PFN_vkQueueEndDebugUtilsLabelEXT    vkQueueEndDebugUtilsLabelEXT_ptr    = nullptr;
    PFN_vkQueueInsertDebugUtilsLabelEXT vkQueueInsertDebugUtilsLabelEXT_ptr = nullptr;
    PFN_vkSetDebugUtilsObjectNameEXT    vkSetDebugUtilsObjectNameEXT_ptr    = nullptr;
    PFN_vkSetDebugUtilsObjectTagEXT     vkSetDebugUtilsObjectTagEXT_ptr     = nullptr;
    PFN_vkSubmitDebugUtilsMessageEXT    vkSubmitDebugUtilsMessageEXT_ptr    = nullptr;

    void StartLabel( VkCommandBuffer cmd, const std::string& name, Vec4 color ) {
        const VkDebugUtilsLabelEXT label = {
                .sType      = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
                .pNext      = nullptr,
                .pLabelName = name.c_str( ),
                .color      = { color[0], color[1], color[2], color[3] },
        };
        vkCmdBeginDebugUtilsLabelEXT( cmd, &label );
    }
    void EndLabel( VkCommandBuffer cmd ) { vkCmdEndDebugUtilsLabelEXT( cmd ); }
} // namespace Debug

// definitions for linkage to the vulkan library
void vkCmdBeginDebugUtilsLabelEXT( VkCommandBuffer commandBuffer, const VkDebugUtilsLabelEXT* pLabelInfo ) {
    return Debug::vkCmdBeginDebugUtilsLabelEXT_ptr( commandBuffer, pLabelInfo );
}

void vkCmdEndDebugUtilsLabelEXT( VkCommandBuffer commandBuffer ) {
    return Debug::vkCmdEndDebugUtilsLabelEXT_ptr( commandBuffer );
}

void vkCmdInsertDebugUtilsLabelEXT( VkCommandBuffer commandBuffer, const VkDebugUtilsLabelEXT* pLabelInfo ) {
    return Debug::vkCmdInsertDebugUtilsLabelEXT_ptr( commandBuffer, pLabelInfo );
}

VkResult vkCreateDebugUtilsMessengerEXT( VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
                                         const VkAllocationCallbacks* pAllocator,
                                         VkDebugUtilsMessengerEXT*    pMessenger ) {
    return Debug::vkCreateDebugUtilsMessengerEXT_ptr( instance, pCreateInfo, pAllocator, pMessenger );
}

void vkDestroyDebugUtilsMessengerEXT( VkInstance instance, VkDebugUtilsMessengerEXT messenger,
                                      const VkAllocationCallbacks* pAllocator ) {
    return Debug::vkDestroyDebugUtilsMessengerEXT_ptr( instance, messenger, pAllocator );
}

void vkQueueBeginDebugUtilsLabelEXT( VkQueue queue, const VkDebugUtilsLabelEXT* pLabelInfo ) {
    return Debug::vkQueueBeginDebugUtilsLabelEXT_ptr( queue, pLabelInfo );
}

void vkQueueEndDebugUtilsLabelEXT( VkQueue queue ) { return Debug::vkQueueEndDebugUtilsLabelEXT_ptr( queue ); }

void vkQueueInsertDebugUtilsLabelEXT( VkQueue queue, const VkDebugUtilsLabelEXT* pLabelInfo ) {
    return Debug::vkQueueInsertDebugUtilsLabelEXT_ptr( queue, pLabelInfo );
}

VkResult vkSetDebugUtilsObjectNameEXT( VkDevice device, const VkDebugUtilsObjectNameInfoEXT* pNameInfo ) {
    return Debug::vkSetDebugUtilsObjectNameEXT_ptr( device, pNameInfo );
}

VkResult vkSetDebugUtilsObjectTagEXT( VkDevice device, const VkDebugUtilsObjectTagInfoEXT* pTagInfo ) {
    return Debug::vkSetDebugUtilsObjectTagEXT_ptr( device, pTagInfo );
}

void vkSubmitDebugUtilsMessageEXT( VkInstance instance, VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                   VkDebugUtilsMessageTypeFlagsEXT             messageTypes,
                                   const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData ) {
    return Debug::vkSubmitDebugUtilsMessageEXT_ptr( instance, messageSeverity, messageTypes, pCallbackData );
}
