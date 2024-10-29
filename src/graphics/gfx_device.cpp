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

#include "tl_vkcontext.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <VkBootstrap.h>
#include <imgui.h>
#include <vk_initializers.h>

#include "vk_engine.h"

using namespace vkb;

constexpr bool B_USE_VALIDATION_LAYERS = true;

ImmediateExecutor::Result<> ImmediateExecutor::Init( TL_VkContext *gfx ) {
    this->m_gfx = gfx;

    // Fence creation
    constexpr VkFenceCreateInfo fence_info = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };
    VKCALL( vkCreateFence( gfx->device, &fence_info, nullptr, &fence ) );

    // Pool and buffer creation
    const VkCommandPoolCreateInfo pool_info = { .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                                .pNext = nullptr,
                                                .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                                .queueFamilyIndex = gfx->graphicsQueueFamily };
    VKCALL( vkCreateCommandPool( gfx->device, &pool_info, nullptr, &pool ) );

    const VkCommandBufferAllocateInfo buffer_info = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                                      .pNext = nullptr,
                                                      .commandPool = pool,
                                                      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                                      .commandBufferCount = 1 };
    VKCALL( vkAllocateCommandBuffers( gfx->device, &buffer_info, &commandBuffer ) );

    return { };
}

void ImmediateExecutor::Execute( std::function<void( VkCommandBuffer cmd )> &&func ) {
    mutex.lock( );

    VKCALL( vkResetFences( m_gfx->device, 1, &fence ) );
    VKCALL( vkResetCommandBuffer( commandBuffer, 0 ) );

    const auto cmd = commandBuffer;
    constexpr VkCommandBufferBeginInfo info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = nullptr,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            .pInheritanceInfo = nullptr,
    };
    VKCALL( vkBeginCommandBuffer( cmd, &info ) );

    func( cmd );

    VKCALL( vkEndCommandBuffer( cmd ) );

    const VkCommandBufferSubmitInfo submit_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
            .pNext = nullptr,
            .commandBuffer = cmd,
            .deviceMask = 0,
    };
    const VkSubmitInfo2 submit = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
            .pNext = nullptr,
            .waitSemaphoreInfoCount = 0,
            .pWaitSemaphoreInfos = nullptr,
            .commandBufferInfoCount = 1,
            .pCommandBufferInfos = &submit_info,
            .signalSemaphoreInfoCount = 0,
            .pSignalSemaphoreInfos = nullptr,
    };

    VKCALL( vkQueueSubmit2( m_gfx->graphicsQueue, 1, &submit, fence ) );
    VKCALL( vkWaitForFences( m_gfx->device, 1, &fence, true, 9999999999 ) );

    mutex.unlock( );
}

void ImmediateExecutor::Cleanup( ) const {
    vkDestroyFence( m_gfx->device, fence, nullptr );
    vkDestroyCommandPool( m_gfx->device, pool, nullptr );
}

TL_VkContext::TL_VkContext( SDL_Window *window ) { Init( window ); }

TL_VkContext::Result<> TL_VkContext::Init( SDL_Window *window ) {
    RETURN_IF_ERROR( InitDevice( window ) );
    RETURN_IF_ERROR( InitAllocator( ) );

    shaderStorage = std::make_unique<ShaderStorage>( this );

    // descriptor pool
    std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> ratios = { { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1.0f },
                                                                       { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1.0f } };
    setPool.Init( device, 100, ratios );

    executor.Init( this );

    imageCodex.Init( this );
    materialCodex.Init( *this );

    // .Init( this, WIDTH, HEIGHT );
    InitSwapchain( WIDTH, HEIGHT );

    return { };
}

void TL_VkContext::Execute( std::function<void( VkCommandBuffer )> &&func ) { executor.Execute( std::move( func ) ); }

GpuBuffer TL_VkContext::Allocate( size_t size, VkBufferUsageFlags usage, VmaMemoryUsage vmaUsage,
                                  const std::string &name ) {
    const VkBufferCreateInfo info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .pNext = nullptr, .size = size, .usage = usage };

    const VmaAllocationCreateInfo vma_info = { .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT |
                                                       VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                                               .usage = vmaUsage };

    GpuBuffer buffer{ };
    VKCALL( vmaCreateBuffer( allocator, &info, &vma_info, &buffer.buffer, &buffer.allocation, &buffer.info ) );
    buffer.name = name;

#ifdef ENABLE_DEBUG_UTILS
    allocationCounter[name]++;
    const VkDebugUtilsObjectNameInfoEXT obj = { .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
                                                .pNext = nullptr,
                                                .objectType = VK_OBJECT_TYPE_BUFFER,
                                                .objectHandle = reinterpret_cast<uint64_t>( buffer.buffer ),
                                                .pObjectName = name.c_str( ) };
    vkSetDebugUtilsObjectNameEXT( device, &obj );
#endif

    return buffer;
}

void TL_VkContext::Free( const GpuBuffer &buffer ) {

#ifdef ENABLE_DEBUG_UTILS
    allocationCounter[buffer.name]--;
#endif

    vmaDestroyBuffer( allocator, buffer.buffer, buffer.allocation );
}

void TL_VkContext::Cleanup( ) {
    CleanupSwapchain( );

    materialCodex.Cleanup( *this );
    imageCodex.Cleanup( );
    meshCodex.Cleanup( *this );
    executor.Cleanup( );
    setPool.DestroyPools( device );
    shaderStorage->Cleanup( );
    vkFreeCommandBuffers( device, computeCommandPool, 1, &computeCommand );
    vkDestroyQueryPool( device, queryPoolTimestamps, nullptr );
    vkDestroyCommandPool( device, computeCommandPool, nullptr );
    vmaDestroyAllocator( allocator );
    vkDestroySurfaceKHR( instance, surface, nullptr );
    vkDestroyDevice( device, nullptr );
    destroy_debug_utils_messenger( instance, debugMessenger );
    vkDestroyInstance( instance, nullptr );
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

VkDescriptorSetLayout TL_VkContext::GetBindlessLayout( ) const { return imageCodex.GetBindlessLayout( ); }

VkDescriptorSet TL_VkContext::GetBindlessSet( ) const { return imageCodex.GetBindlessSet( ); }

float TL_VkContext::GetTimestampInMs( uint64_t start, uint64_t end ) const {
    const auto period = deviceProperties.properties.limits.timestampPeriod;
    return ( end - start ) * period / 1000000.0f;
}

void TL_VkContext::SetObjectDebugName( VkObjectType type, void *object, const std::string &name ) {
#ifdef ENABLE_DEBUG_FEATURES
    const VkDebugUtilsObjectNameInfoEXT obj = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .pNext = nullptr,
        .objectType = type,
        .objectHandle = reinterpret_cast<uint64_t>( object ),
        .pObjectName = name.c_str( ),
    };
    vkSetDebugUtilsObjectNameEXT( device, &obj );
#endif
}

TL_FrameData &TL_VkContext::GetCurrentFrame( ) { return frames.at( frameNumber % FrameOverlap ); }

void TL_VkContext::DrawDebug( ) const {
    auto props = deviceProperties.properties;
    const auto limits = props.limits;

    ImGui::Text( "Device Name: %s", props.deviceName );
    ImGui::Text( "Driver Version: %d.%d.%d", VK_VERSION_MAJOR( props.driverVersion ),
                 VK_VERSION_MINOR( props.driverVersion ), VK_VERSION_PATCH( props.driverVersion ) );

    VkDeviceSize total_vram = 0;
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

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback( VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                              VkDebugUtilsMessageTypeFlagsEXT messageType,
                                              const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                                              void *pUserData ) {

    auto &engine = TL_Engine::Get( );
    engine.console.AddLog( "{}", pCallbackData->pMessage );

    return VK_FALSE;
}

TL_VkContext::Result<> TL_VkContext::InitDevice( SDL_Window *window ) {
    // Instance
    InstanceBuilder builder;
    auto instance_prototype = builder.set_app_name( "Vulkan Engine" )
                                      .request_validation_layers( B_USE_VALIDATION_LAYERS )
                                      .enable_extension( "VK_EXT_debug_utils" )
                                      .set_debug_callback( debugCallback )
                                      .require_api_version( 1, 3, 0 )
                                      .build( );

    if ( !instance_prototype.has_value( ) ) {
        return std::unexpected( GfxDeviceError{ Error::InstanceCreationFailed } );
    }

    auto &bs_instance = instance_prototype.value( );

    instance = bs_instance.instance;

    // Surface
    SDL_Vulkan_CreateSurface( window, instance, &surface );

    // Device
    VkPhysicalDeviceVulkan13Features features_13{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
            .synchronization2 = true,
            .dynamicRendering = true,
    };
    VkPhysicalDeviceVulkan12Features features_12{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
            .descriptorIndexing = true,
            .descriptorBindingSampledImageUpdateAfterBind = true,
            .descriptorBindingPartiallyBound = true,
            .runtimeDescriptorArray = true,
            .hostQueryReset = true,
            .bufferDeviceAddress = true,
    };
    VkPhysicalDeviceVulkan11Features features_11{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
            .multiview = true,
    };
    VkPhysicalDeviceFeatures features{
            .fillModeNonSolid = true,
            .samplerAnisotropy = true,
    };

    PhysicalDeviceSelector selector{ bs_instance };
    auto physical_device_res = selector.set_minimum_version( 1, 3 )
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

    auto &physical_device = physical_device_res.value( );
    chosenGpu = physical_device;

    for ( const auto &ext : physical_device.get_available_extensions( ) ) {
        TL_Engine::Get( ).console.AddLog( "{}", ext.c_str( ) );
    }

    deviceProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    vkGetPhysicalDeviceProperties2( physical_device, &deviceProperties );

    assert( deviceProperties.properties.limits.timestampPeriod != 0 &&
            "Timestamp queries not supported on this device!" );

    vkGetPhysicalDeviceMemoryProperties( chosenGpu, &memProperties );

    // Logical Device
    DeviceBuilder device_builder{ physical_device };
    auto device_res = device_builder.build( );
    if ( !device_res ) {
        return std::unexpected( GfxDeviceError{ Error::LogicalDeviceCreationFailed } );
    }

    auto &bs_device = device_res.value( );
    device = bs_device;

    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties( physical_device.physical_device, &properties );

    // Access maxDescriptorSetCount
    uint32_t max_descriptor_set_count = properties.limits.maxDescriptorSetSampledImages;
    TL_Engine::Get( ).console.AddLog( "Max sampled images: {}", max_descriptor_set_count );

    // Queue
    graphicsQueue = bs_device.get_queue( QueueType::graphics ).value( );
    graphicsQueueFamily = bs_device.get_queue_index( QueueType::graphics ).value( );

    computeQueue = bs_device.get_queue( QueueType::compute ).value( );
    computeQueueFamily = bs_device.get_queue_index( QueueType::compute ).value( );
    const VkCommandPoolCreateInfo command_pool_info =
            vk_init::CommandPoolCreateInfo( computeQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT );
    VKCALL( vkCreateCommandPool( device, &command_pool_info, nullptr, &computeCommandPool ) );
    VkCommandBufferAllocateInfo cmd_alloc_info = vk_init::CommandBufferAllocateInfo( computeCommandPool, 1 );
    VKCALL( vkAllocateCommandBuffers( device, &cmd_alloc_info, &computeCommand ) );

    // Query Pool
    VkQueryPoolCreateInfo query_pool_create_info = {
            .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
            .pNext = nullptr,
            .queryType = VK_QUERY_TYPE_TIMESTAMP,
            .queryCount = static_cast<uint32_t>( gpuTimestamps.size( ) ),
    };
    VKCALL( vkCreateQueryPool( device, &query_pool_create_info, nullptr, &queryPoolTimestamps ) );

    InitDebugFunctions( );

    return { };
}

TL_VkContext::Result<> TL_VkContext::InitAllocator( ) {
    const VmaAllocatorCreateInfo info = {
            .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
            .physicalDevice = chosenGpu,
            .device = device,
            .instance = instance,
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

    auto swapchain_res = builder.set_desired_format( VkSurfaceFormatKHR{
                                                             .format = format,
                                                             .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
                                                     } )
                                 .set_desired_present_mode( presentMode )
                                 .add_image_usage_flags( VK_IMAGE_USAGE_TRANSFER_DST_BIT )
                                 .build( );

    // TODO: throw instead
    assert( swapchain_res.has_value( ) );

    auto &bs_swapchain = swapchain_res.value( );

    swapchain = bs_swapchain.swapchain;
    images = bs_swapchain.get_images( ).value( );
    views = bs_swapchain.get_image_views( ).value( );

    assert( images.size( ) != 0 );
    assert( views.size( ) != 0 );

    const VkCommandPoolCreateInfo command_pool_info =
            vk_init::CommandPoolCreateInfo( graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT );
    for ( int i = 0; i < FrameOverlap; i++ ) {
        VKCALL( vkCreateCommandPool( device, &command_pool_info, nullptr, &frames[i].pool ) );

        VkCommandBufferAllocateInfo cmd_alloc_info = vk_init::CommandBufferAllocateInfo( frames[i].pool, 1 );
        VKCALL( vkAllocateCommandBuffers( device, &cmd_alloc_info, &frames[i].commandBuffer ) );

        const VkDebugUtilsObjectNameInfoEXT obj = {
                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
                .pNext = nullptr,
                .objectType = VK_OBJECT_TYPE_COMMAND_BUFFER,
                .objectHandle = reinterpret_cast<uint64_t>( frames[i].commandBuffer ),
                .pObjectName = "Main CMD",
        };
        vkSetDebugUtilsObjectNameEXT( device, &obj );
    }

    auto fenceCreateInfo = vk_init::FenceCreateInfo( VK_FENCE_CREATE_SIGNALED_BIT );
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
    const VkExtent3D extent = { .width = this->extent.width, .height = this->extent.height, .depth = 1 };

    // TODO: transition to correct layout ( check validation layers )
    for ( auto &frame : frames ) {
        std::vector<unsigned char> empty_image_data;
        empty_image_data.resize( extent.width * extent.height * 8, 0 );
        frame.hdrColor = imageCodex.LoadImageFromData( "hdr image pbr", empty_image_data.data( ), draw_image_extent,
                                                       VK_FORMAT_R16G16B16A16_SFLOAT, draw_image_usages, false );

        frame.postProcessImage =
                imageCodex.CreateEmptyImage( "post process", draw_image_extent, VK_FORMAT_R8G8B8A8_UNORM,
                                             draw_image_usages | VK_IMAGE_USAGE_STORAGE_BIT, false );

        empty_image_data.resize( extent.width * extent.height * 4, 0 );
        frame.depth = imageCodex.LoadImageFromData( "main depth image", empty_image_data.data( ), draw_image_extent,
                                                    VK_FORMAT_D32_SFLOAT, depth_image_usages, false );
        std::vector<unsigned char> empty_data;
        empty_data.resize( extent.width * extent.height * 4 * 2, 0 );

        frame.gBuffer.position = imageCodex.LoadImageFromData( "gbuffer.position", empty_data.data( ), extent,
                                                               VK_FORMAT_R16G16B16A16_SFLOAT, usages, false );
        frame.gBuffer.normal = imageCodex.LoadImageFromData( "gbuffer.normal", empty_data.data( ), extent,
                                                             VK_FORMAT_R16G16B16A16_SFLOAT, usages, false );
        frame.gBuffer.pbr = imageCodex.LoadImageFromData( "gbuffer.pbr", empty_data.data( ), extent,
                                                          VK_FORMAT_R16G16B16A16_SFLOAT, usages, false );
        frame.gBuffer.albedo = imageCodex.LoadImageFromData( "gbuffer.albedo", empty_data.data( ), extent,
                                                             VK_FORMAT_R16G16B16A16_SFLOAT, usages, false );
    }
}
void TL_VkContext::CleanupSwapchain( ) {
    for ( uint64_t i = 0; i < FrameOverlap; i++ ) {
        vkDestroyCommandPool( device, frames[i].pool, nullptr );

        // sync objects
        vkDestroyFence( device, frames[i].fence, nullptr );
        vkDestroySemaphore( device, frames[i].renderSemaphore, nullptr );
        vkDestroySemaphore( device, frames[i].swapchainSemaphore, nullptr );

        frames[i].deletionQueue.Flush( );
    }

    vkDestroySwapchainKHR( device, swapchain, nullptr );

    for ( const auto &view : views ) {
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
    PFN_vkCmdBeginDebugUtilsLabelEXT vkCmdBeginDebugUtilsLabelEXT_ptr = nullptr;
    PFN_vkCmdEndDebugUtilsLabelEXT vkCmdEndDebugUtilsLabelEXT_ptr = nullptr;
    PFN_vkCmdInsertDebugUtilsLabelEXT vkCmdInsertDebugUtilsLabelEXT_ptr = nullptr;
    PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT_ptr = nullptr;
    PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT_ptr = nullptr;
    PFN_vkQueueBeginDebugUtilsLabelEXT vkQueueBeginDebugUtilsLabelEXT_ptr = nullptr;
    PFN_vkQueueEndDebugUtilsLabelEXT vkQueueEndDebugUtilsLabelEXT_ptr = nullptr;
    PFN_vkQueueInsertDebugUtilsLabelEXT vkQueueInsertDebugUtilsLabelEXT_ptr = nullptr;
    PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectNameEXT_ptr = nullptr;
    PFN_vkSetDebugUtilsObjectTagEXT vkSetDebugUtilsObjectTagEXT_ptr = nullptr;
    PFN_vkSubmitDebugUtilsMessageEXT vkSubmitDebugUtilsMessageEXT_ptr = nullptr;

    void StartLabel( VkCommandBuffer cmd, const std::string &name, Vec4 color ) {
        const VkDebugUtilsLabelEXT label = {
                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
                .pNext = nullptr,
                .pLabelName = name.c_str( ),
                .color = { color[0], color[1], color[2], color[3] },
        };
        vkCmdBeginDebugUtilsLabelEXT( cmd, &label );
    }
    void EndLabel( VkCommandBuffer cmd ) { vkCmdEndDebugUtilsLabelEXT( cmd ); }
} // namespace Debug

// definitions for linkage to the vulkan library
void vkCmdBeginDebugUtilsLabelEXT( VkCommandBuffer commandBuffer, const VkDebugUtilsLabelEXT *pLabelInfo ) {
    return Debug::vkCmdBeginDebugUtilsLabelEXT_ptr( commandBuffer, pLabelInfo );
}

void vkCmdEndDebugUtilsLabelEXT( VkCommandBuffer commandBuffer ) {
    return Debug::vkCmdEndDebugUtilsLabelEXT_ptr( commandBuffer );
}

void vkCmdInsertDebugUtilsLabelEXT( VkCommandBuffer commandBuffer, const VkDebugUtilsLabelEXT *pLabelInfo ) {
    return Debug::vkCmdInsertDebugUtilsLabelEXT_ptr( commandBuffer, pLabelInfo );
}

VkResult vkCreateDebugUtilsMessengerEXT( VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
                                         const VkAllocationCallbacks *pAllocator,
                                         VkDebugUtilsMessengerEXT *pMessenger ) {
    return Debug::vkCreateDebugUtilsMessengerEXT_ptr( instance, pCreateInfo, pAllocator, pMessenger );
}

void vkDestroyDebugUtilsMessengerEXT( VkInstance instance, VkDebugUtilsMessengerEXT messenger,
                                      const VkAllocationCallbacks *pAllocator ) {
    return Debug::vkDestroyDebugUtilsMessengerEXT_ptr( instance, messenger, pAllocator );
}

void vkQueueBeginDebugUtilsLabelEXT( VkQueue queue, const VkDebugUtilsLabelEXT *pLabelInfo ) {
    return Debug::vkQueueBeginDebugUtilsLabelEXT_ptr( queue, pLabelInfo );
}

void vkQueueEndDebugUtilsLabelEXT( VkQueue queue ) { return Debug::vkQueueEndDebugUtilsLabelEXT_ptr( queue ); }

void vkQueueInsertDebugUtilsLabelEXT( VkQueue queue, const VkDebugUtilsLabelEXT *pLabelInfo ) {
    return Debug::vkQueueInsertDebugUtilsLabelEXT_ptr( queue, pLabelInfo );
}

VkResult vkSetDebugUtilsObjectNameEXT( VkDevice device, const VkDebugUtilsObjectNameInfoEXT *pNameInfo ) {
    return Debug::vkSetDebugUtilsObjectNameEXT_ptr( device, pNameInfo );
}

VkResult vkSetDebugUtilsObjectTagEXT( VkDevice device, const VkDebugUtilsObjectTagInfoEXT *pTagInfo ) {
    return Debug::vkSetDebugUtilsObjectTagEXT_ptr( device, pTagInfo );
}

void vkSubmitDebugUtilsMessageEXT( VkInstance instance, VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                   VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                                   const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData ) {
    return Debug::vkSubmitDebugUtilsMessageEXT_ptr( instance, messageSeverity, messageTypes, pCallbackData );
}
