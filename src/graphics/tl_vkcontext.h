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

#include <expected>
#include <mutex>
#include <vk_types.h>

#include "descriptors.h"
#include "gbuffer.h"
#include "image_codex.h"
#include "material_codex.h"
#include "mesh_codex.h"

#include <graphics/resources/tl_pipeline.h>
#include "shader_storage.h"

class TL_VkContext;
class ShaderStorage;

#ifdef ENABLE_DEBUG_UTILS
#define START_LABEL( cmd, name, color ) Debug::StartLabel( cmd, name, color )
#define END_LABEL( cmd ) Debug::EndLabel( cmd )
#else
#define START_LABEL( cmd, name, color )
#define END_LABEL
#endif

namespace Debug {
    extern PFN_vkCmdBeginDebugUtilsLabelEXT    vkCmdBeginDebugUtilsLabelEXT_ptr;
    extern PFN_vkCmdEndDebugUtilsLabelEXT      vkCmdEndDebugUtilsLabelEXT_ptr;
    extern PFN_vkCmdInsertDebugUtilsLabelEXT   vkCmdInsertDebugUtilsLabelEXT_ptr;
    extern PFN_vkCreateDebugUtilsMessengerEXT  vkCreateDebugUtilsMessengerEXT_ptr;
    extern PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT_ptr;
    extern PFN_vkQueueBeginDebugUtilsLabelEXT  vkQueueBeginDebugUtilsLabelEXT_ptr;
    extern PFN_vkQueueEndDebugUtilsLabelEXT    vkQueueEndDebugUtilsLabelEXT_ptr;
    extern PFN_vkQueueInsertDebugUtilsLabelEXT vkQueueInsertDebugUtilsLabelEXT_ptr;
    extern PFN_vkSetDebugUtilsObjectNameEXT    vkSetDebugUtilsObjectNameEXT_ptr;
    extern PFN_vkSetDebugUtilsObjectTagEXT     vkSetDebugUtilsObjectTagEXT_ptr;
    extern PFN_vkSubmitDebugUtilsMessageEXT    vkSubmitDebugUtilsMessageEXT_ptr;

    void StartLabel( VkCommandBuffer cmd, const std::string &name, Vec4 color );
    void EndLabel( VkCommandBuffer cmd );
} // namespace Debug

struct ImmediateExecutor {
    VkFence         fence;
    VkCommandBuffer commandBuffer;
    VkCommandPool   pool;

    std::mutex mutex;

    enum class Error {

    };

    template<typename T = void>
    using Result = std::expected<T, Error>;

    Result<> Init( TL_VkContext *gfx );
    void     Execute( std::function<void( VkCommandBuffer )> &&func );
    void     Cleanup( ) const;

private:
    TL_VkContext *m_gfx = nullptr;
};

struct TL_FrameData {
    VkCommandPool   pool;
    VkCommandBuffer commandBuffer;
    VkSemaphore     swapchainSemaphore;
    VkSemaphore     renderSemaphore;
    VkFence         fence;
    DeletionQueue   deletionQueue;
    ImageId         hdrColor;
    ImageId         postProcessImage;
    ImageId         depth;
    GBuffer         gBuffer;

    VkQueryPool              queryPoolTimestamps = nullptr;
    std::array<uint64_t, 10> gpuTimestamps;
};

class TL_VkContext {
public:
    using Ptr = TL_VkContext *;
    using Ref = std::shared_ptr<TL_VkContext>;

    enum class Error {
        InstanceCreationFailed,
        PhysicalDeviceSelectionFailed,
        LogicalDeviceCreationFailed,
        GlobalAllocatorFailed,
    };

    struct GfxDeviceError {
        Error error;
    };

    template<typename T = void>
    using Result = std::expected<T, GfxDeviceError>;

             TL_VkContext( ) = default;
    explicit TL_VkContext( struct SDL_Window *window );

    Result<>  Init( );
    void      Execute( std::function<void( VkCommandBuffer )> &&func );
    GpuBuffer Allocate( size_t size, VkBufferUsageFlags usage, VmaMemoryUsage vmaUsage, const std::string &name );
    void      Free( const GpuBuffer &buffer );
    void      Cleanup( );

    // Resources
    std::shared_ptr<TL::Pipeline> GetOrCreatePipeline( const TL::PipelineConfig &config );

    VkDescriptorSet    AllocateSet( VkDescriptorSetLayout layout );
    MultiDescriptorSet AllocateMultiSet( VkDescriptorSetLayout layout );

    VkDescriptorSetLayout GetBindlessLayout( ) const;
    VkDescriptorSet       GetBindlessSet( ) const;

    float GetTimestampInMs( uint64_t start, uint64_t end ) const;

    void SetObjectDebugName( VkObjectType type, void *object, const std::string &name );

    DescriptorAllocatorGrowable setPool;

    VkInstance       instance  = VK_NULL_HANDLE;
    VkPhysicalDevice chosenGpu = VK_NULL_HANDLE;
    VkDevice         device    = VK_NULL_HANDLE;

    VkPhysicalDeviceProperties2      deviceProperties = { };
    VkPhysicalDeviceMemoryProperties memProperties    = { };

    VkDebugUtilsMessengerEXT                  debugMessenger = { };
    std::unordered_map<std::string, uint64_t> allocationCounter;

    VkQueue  graphicsQueue;
    uint32_t graphicsQueueFamily;

    // TODO: implement a command buffer manager (list of commands to get pulled/pushed)
    VkQueue         computeQueue;
    uint32_t        computeQueueFamily;
    VkCommandPool   computeCommandPool;
    VkCommandBuffer computeCommand;

    ImmediateExecutor executor;
    VmaAllocator      allocator;

    VkSurfaceKHR surface;

    ImageCodex        imageCodex;
    TL::MaterialCodex materialCodex;
    MeshCodex         meshCodex;

    std::unique_ptr<ShaderStorage> shaderStorage;

    // Swapchain
    static constexpr u32                   FrameOverlap = 2;
    VkSwapchainKHR                         swapchain    = VK_NULL_HANDLE;
    std::array<TL_FrameData, FrameOverlap> frames;
    std::vector<VkImage>                   images;
    std::vector<VkImageView>               views;
    VkFormat                               format;
    VkExtent2D                             extent;
    VkPresentModeKHR                       presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;

    u64           frameNumber = 0;
    TL_FrameData &GetCurrentFrame( );

    void DrawDebug( ) const;

private:
    Result<> InitDevice( struct SDL_Window *window );
    Result<> InitAllocator( );
    void     InitSwapchain( u32 width, u32 height );
    void     CleanupSwapchain( );
    void     InitDebugFunctions( ) const;

    std::unordered_map<std::string_view, std::shared_ptr<TL::Pipeline>> m_pipelines;


    DeletionQueue m_deletionQueue;
};

namespace TL {
    extern std::unique_ptr<TL_VkContext> vkctx;
}
