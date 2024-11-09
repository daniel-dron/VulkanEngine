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
#include <graphics/utils/vk_types.h>
#include <mutex>

#include <graphics/resources/r_buffer.h>
#include <graphics/resources/r_image.h>

#include "../descriptors.h"
#include "../gbuffer.h"
#include "../shader_storage.h"


struct Material;
namespace TL {
    class Pipeline;
    struct PipelineConfig;
} // namespace TL
class TL_VkContext;
class ShaderStorage;

#ifdef ENABLE_DEBUG_UTILS
#define START_LABEL( cmd, name, color ) Debug::StartLabel( cmd, name, color )
#define END_LABEL( cmd ) Debug::EndLabel( cmd )
#else
#define START_LABEL( cmd, name, color )
#define END_LABEL( cmd )
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

    void StartLabel( VkCommandBuffer cmd, const std::string& name, Vec4 color );
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

    Result<> Init( TL_VkContext* gfx );
    void     Execute( std::function<void( VkCommandBuffer )>&& func );
    void     Cleanup( ) const;

private:
    TL_VkContext* m_gfx = nullptr;
};

//==============================================================================//
//                             Materials                                        //
//==============================================================================//

namespace TL {
    class Renderer;
}

namespace TL::renderer {

    struct MaterialHandle {
        u16 index;      // Index into material data array
        u16 generation; // Generation counter for lifetime validation
    };

    struct MaterialData {
        Vec4    BaseColor;         // 16 bytes
        Vec4    EmissiveColor;     // 16 bytes
        Vec4    Factors;           // 16 bytes ( metalness, roughness, N/A, N/A )
        ImageId TextureIndices[4]; // 16 bytes ( albedo, mr, normal, N/A )
    };

    constexpr u32 MAX_MATERIALS = 1024;

    class MaterialPool {
    public:
        void Init( );
        void Shutdown( );

        MaterialHandle               CreateMaterial( const Material& material );
        void                         DestroyMaterial( MaterialHandle handle );
        MaterialData&                GetMaterial( MaterialHandle handle );     // Asserted fetch. Recommended only for pre validated and hot paths (renderer code)
        std::optional<MaterialData*> GetMaterialSafe( MaterialHandle handle ); // Returns possible material if present. Not recommended for hot paths.
        void                         UpdateMaterial( MaterialHandle handle );  // Update material data for the gpu. Updating the material must be done
                                                                               // through a reference gotten from GetMaterial or GetMaterialSafe.
        bool                         IsValid( MaterialHandle handle ) const;   // Check if the index is valid and it matches its current generation

    private:
        std::array<MaterialData, MAX_MATERIALS> m_materialDatas = { }; // Material data mapped directly to gpu memory
                                                                       // (Hot data, accessed every frame by the game)
        std::array<u16, MAX_MATERIALS>          m_generations   = { }; // This indicates how many times the given handle was allocated.
                                                                       // The same handle ID but different generation means the older one is invalid.
        std::vector<u16>                        m_freeIndices   = { }; // Filled with free indices. At the start, it contains all indices into m_materialDatas backwards

        std::unique_ptr<Buffer> m_materialsGpuBuffer = nullptr;

        friend class Renderer;
    };

} // namespace TL::renderer

//==============================================================================//
//==============================================================================//


//==============================================================================//
//                                 Meshes                                       //
//==============================================================================//

namespace TL::renderer {

    struct MeshHandle {
        u16 index;      // Index into mesh data array
        u16 generation; // Generation counter for lifetime validation
    };

    struct AABB {
        Vec3 min;
        Vec3 max;
    };

    struct Vertex {
        Vec4 Position;  // 16 bytes (Vec3 position | z = float UV_X)
        Vec4 Normal;    // 16 bytes (Vec3 normal | z = float UV_Y)
        Vec4 Tangent;   // 16 bytes (Vec3 tangent)
        Vec4 Bitangent; // 16 bytes (Vec3 bitangeng)
    };

    // Contents of a mesh includign vertex data and indices
    struct MeshContent {
        std::vector<Vertex> Vertices; // Vertices for generic meshes with position, uvs, normal and tangent data.
        std::vector<u32>    Indices;  // Indices of the mesh in a triangle strip.
        AABB                Aabb;     // Axis-Aligned Bunding Box for the mesh. Used in frustum culling.
    };

    // Data needed for every frame rendering. Contains index and vertex buffers and its index count. Also AABB for frustum culling calculations.
    struct MeshData {
        std::unique_ptr<Buffer> IndexBuffer  = nullptr;
        std::unique_ptr<Buffer> VertexBuffer = nullptr;
        MeshContent             Content;
        u32                     IndexCount;
    };

    constexpr u32 MAX_MESHES = 2048;

    class MeshPool {
    public:
        void Init( );
        void Shutdown( );

        MeshHandle               CreateMesh( const MeshContent& content );
        void                     DestroyMesh( MeshHandle handle );
        MeshData&                GetMesh( MeshHandle handle );       // Asserted fetch. Recommended only for pre validated and hot paths (renderer code)
        std::optional<MeshData*> GetMeshSafe( MeshHandle handle );   // Returns possible material if present. Not recommended for hot paths.
        bool                     IsValid( MeshHandle handle ) const; // Check if the index is valid and it matches its current generation
    private:
        std::array<MeshData, MAX_MESHES> m_meshDatas   = { };
        std::array<u16, MAX_MESHES>      m_generations = { }; // This indicates how many times the given handle was allocated.
                                                              // The same handle ID but different generation means the older one is invalid.
        u32                              m_meshCount   = 0;   // Keeps track of the number of meshes in the pool for stats.
        std::vector<u16>                 m_freeIndices = { }; // Filled with free indices. At the start, it contains all indices into m_meshDatas backwards

        friend class Renderer;
    };

} // namespace TL::renderer

//==============================================================================//
//==============================================================================//


//==============================================================================//
//                             VK CONTEXT                                       //
//==============================================================================//


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

// Vulkan stuff that most of the render code will want to access at any given time.
// Its lifetime is global.
class TL_VkContext {
public:
    using Ptr = TL_VkContext*;
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
    explicit TL_VkContext( struct SDL_Window* window );

    Result<> Init( );
    void     RecreateSwapchain( u32 width, u32 height );
    void     Execute( std::function<void( VkCommandBuffer )>&& func );
    void     Cleanup( );

    // Resources
    std::shared_ptr<TL::Pipeline> GetOrCreatePipeline( const TL::PipelineConfig& config );

    VkDescriptorSet    AllocateSet( VkDescriptorSetLayout layout );
    MultiDescriptorSet AllocateMultiSet( VkDescriptorSetLayout layout );

    VkDescriptorSetLayout GetBindlessLayout( ) const;
    VkDescriptorSet       GetBindlessSet( ) const;

    float GetTimestampInMs( uint64_t start, uint64_t end ) const;

    void SetObjectDebugName( VkObjectType type, void* object, const std::string& name );

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

    // Resources
    TL::renderer::MaterialPool MaterialPool;
    TL::renderer::MeshPool     MeshPool;
    TL::renderer::ImageCodex   ImageCodex;

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
    u64                                    frameNumber = 0;
    TL_FrameData&                          GetCurrentFrame( );

    void DrawDebug( ) const;

private:
    Result<> InitDevice( struct SDL_Window* window );
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
