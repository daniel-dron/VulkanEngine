// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include "camera.h"
#include "vk_descriptors.h"
#include "vk_loader.h"
#include <cstdint>
#include <memory>
#include <span>
#include <vk_types.h>
#include <vulkan/vulkan_core.h>

#include <deque>
#include <functional>

struct DeletionQueue
{
  std::deque<std::function<void()>> deletors;

  void push_function(std::function<void()> &&deletor)
  {
    deletors.push_back(deletor);
  }

  void flush()
  {
    // reverse iterate the deletion queue to execute all the functions
    for (auto it = deletors.rbegin(); it != deletors.rend(); it++)
    {
      (*it)(); // call functors
    }

    deletors.clear();
  }
};

struct FrameData
{
  VkCommandPool _commandPool;
  VkCommandBuffer _mainCommandBuffer;
  VkSemaphore _swapchainSemaphore, _renderSemaphore;
  VkFence _renderFence;
  DeletionQueue _deletionQueue;
  DescriptorAllocatorGrowable _frameDescriptors;
};
constexpr unsigned int FRAME_OVERLAP = 3;

struct ComputePushConstants
{
  glm::vec4 data1;
  glm::vec4 data2;
  glm::vec4 data3;
  glm::vec4 data4;
};

struct ComputeEffect
{
  const char *name;

  VkPipeline pipeline;
  VkPipelineLayout layout;

  ComputePushConstants data;
};

struct GPUSceneData
{
  glm::mat4 view;
  glm::mat4 proj;
  glm::mat4 viewproj;
  glm::vec4 ambientColor;
  glm::vec4 sunlightDirection; // w for sun power
  glm::vec4 sunlightColor;
};

class VulkanEngine;

struct GLTFMetallic_Roughness
{
  MaterialPipeline opaquePipeline;
  MaterialPipeline transparentPipeline;

  VkDescriptorSetLayout materialLayout;

  struct MaterialConstants
  {
    glm::vec4 colorFactors;
    glm::vec4 metal_rough_factors;
    // padding?
    glm::vec4 extra[14];
  };

  struct MaterialResources
  {
    AllocatedImage colorImage;
    VkSampler colorSampler;
    AllocatedImage metalRoughImage;
    VkSampler metalRoughSampler;
    VkBuffer dataBuffer; // material constants
    uint32_t dataBufferOffset;
  };

  DescriptorWriter writer;

  void build_pipelines(VulkanEngine *engine);
  void clear_resources(VkDevice device);

  MaterialInstance
  write_material(VkDevice device, MaterialPass pass,
                 const MaterialResources &resources,
                 DescriptorAllocatorGrowable &descriptorAllocator);
};

struct RenderObject
{
  uint32_t indexCount;
  uint32_t firstIndex;
  VkBuffer indexBuffer;

  MaterialInstance *material;
  Bounds bounds;
  glm::mat4 transform;
  VkDeviceAddress vertexBufferAddress;
};

struct DrawContext
{
  std::vector<RenderObject> OpaqueSurfaces;
  std::vector<RenderObject> TransparentSurfaces;
};

struct MeshNode : public Node
{
  std::shared_ptr<MeshAsset> mesh;

  virtual void Draw(const glm::mat4 &topMatrix, DrawContext &ctx) override;
};

struct EngineStats
{
  float frametime;
  int triangle_count;
  int drawcall_count;
  float scene_update_time;
  float mesh_draw_time;
};

/// @brief This is the main Vulkan Engine class
class VulkanEngine
{
public:
  bool _isInitialized{false};
  int _frameNumber{0};
  bool stop_rendering{false};
  VkExtent2D _windowExtent{1700, 900};
  EngineStats stats;

  struct SDL_Window *_window{nullptr};

  static VulkanEngine &Get();

  /// @brief Does most of the engine initialization 
  void init();

  // shuts down the engine
  void cleanup();

  // draw loop
  void draw();

  // run main loop
  void run();

  void immediateSubmit(std::function<void(VkCommandBuffer cmd)> &&function);
  void drawImgui(VkCommandBuffer cmd, VkImageView targetImageView);
  AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage,
                                VmaMemoryUsage memoryUsage);
  void destroy_buffer(const AllocatedBuffer &buffer);
  AllocatedImage create_image(VkExtent3D size, VkFormat format,
                              VkImageUsageFlags usage, bool mipmapped = false);
  AllocatedImage create_image(void *data, VkExtent3D size, VkFormat format,
                              VkImageUsageFlags usage, bool mipmapped = false);
  void destroy_image(const AllocatedImage &img);

  /// @brief Uploads mesh data to gpu buffers
  /// @param indices list of indices in uint32_t format
  /// @param vertices list of vertices
  /// @return returns a struct containing the allocated index and vertex buffer and its corresponding gpu address
  GPUMeshBuffers uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices);

  // vulkan stuff
  VkInstance _instance;
  VkDebugUtilsMessengerEXT _debug_messenger;
  VkPhysicalDevice _chosenGPU;
  VkDevice _device;
  VkSurfaceKHR _surface;
  DeletionQueue _mainDeletionQueue;
  VmaAllocator _allocator;

  VkQueue _graphicsQueue;
  uint32_t _graphicsQueueFamily;

  // Used as the color attachement for actual rendering
  // will be copied into the final swapchain image
  AllocatedImage _drawImage;
  AllocatedImage _depthImage;
  VkExtent2D _drawExtent;
  float renderScale = 1.f;

  void *texID;

  //
  // swapchain
  //
  VkSwapchainKHR _swapchain;
  VkFormat _swapchainImageFormat;

  std::vector<VkImage> _swapchainImages;
  std::vector<VkImageView> _swapchainImageViews;
  VkExtent2D _swapchainExtent;

  //
  // command
  //
  FrameData _frames[FRAME_OVERLAP];
  FrameData &get_current_frame()
  {
    return _frames[_frameNumber % FRAME_OVERLAP];
  }

  //
  // descriptors
  //
  DescriptorAllocatorGrowable globalDescriptorAllocator;

  VkDescriptorSet _drawImageDescriptors;
  VkDescriptorSetLayout _drawImageDescriptorLayout;

  //
  // Pipeline
  //
  // VkPipeline _gradientPipeline;
  VkPipelineLayout _gradientPipelineLayout;

  //
  // immediate commands
  //
  VkFence _immFence;
  VkCommandBuffer _immCommandBuffer;
  VkCommandPool _immCommandPool;

  std::vector<ComputeEffect> backgroundEffects;
  int currentBackgroundEffect{0};

  //
  // scene
  //
  GPUSceneData sceneData;
  bool enableFrustumCulling = true;
  VkDescriptorSetLayout _gpuSceneDataDescriptorLayout;

  AllocatedImage _whiteImage;
  AllocatedImage _blackImage;
  AllocatedImage _greyImage;
  AllocatedImage _errorCheckerboardImage;

  VkSampler _defaultSamplerLinear;
  VkSampler _defaultSamplerNearest;

  MaterialInstance defaultData;
  GLTFMetallic_Roughness metalRoughMaterial;

  DrawContext mainDrawContext;

  std::unordered_map<std::string, std::shared_ptr<LoadedGLTF>> loadedScenes;
  Camera mainCamera;

private:
  /// @brief Initializes SDL context and creates SDL window
  void init_sdl();
  
  /// @brief Initializes core vulkan resources
  void init_vulkan();

  /// @brief Initializes vulkan device and its queues
  void init_device();

  /// @brief Initializes VMA
  void init_allocator();

  /// @brief Initializes swapchain.
  /// Creates an intermediate draw image where the actual frame rendering is done onto.
  /// The contents are then blipped to the swap chain image at the end of the frame.
  /// Also creates a depth image.
  void init_swapchain();

  /// @brief Initializes a command pool for each inflight frame of the swapchain.
  /// Also creates an immediate command pool that is used to execute immediate commands
  /// on the gpu. Usefull for ImGui and other generic purposes.
  void init_commands();

  /// @brief Creates a synchronization structures.
  /// Creates a fence for the immediate command pool.
  /// Creates a fence and two semaphores for each inflight frame of the swapchain.
  void init_sync_structures();

  /// @brief Creates pipeline descriptors and its allocators.
  /// Creates a growable descriptor allocator for each inflight frame.
  /// Creates a descriptor set for the background compute shader.
  /// Creates a descriptor set for scene data.
  void init_descriptors();

  /// @brief Creates general pipelines.
  /// Creates the background compute pipeline.
  /// Creates the PBR Metalness pipeline.
  void init_pipelines();

  /// @brief Creates the background compute pipeline.
  void init_background_pipelines();

  /// @brief Initializes ImGui entire context
  void init_imgui();

  /// @brief Dispatches compute shader to fill in main image
  /// @param cmd VkCommandBuffer that will queue in the work
  void draw_background(VkCommandBuffer cmd);

  /// @brief Responsible for queueing commands to render all Renderables.
  /// Sorts based on material and performs frustum culling.
  /// @param cmd VkCommandBuffer that will queue in the work
  void draw_geometry(VkCommandBuffer cmd);

  /// @brief Initializes default white, black, error images and its samplers
  void init_default_data();
  void init_images();

  /// @brief Updates scene data and call Draw on each scene node.
  void update_scene();

  void create_swapchain(uint32_t width, uint32_t height,
                        VkSwapchainKHR old = VK_NULL_HANDLE);
  void resize_swapchain(uint32_t width, uint32_t height);
  void destroy_swapchain();
};
