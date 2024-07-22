// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.h>
#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <span>

#include "camera/camera.h"
#include "vk_descriptors.h"
#include "vk_loader.h"
#include "graphics/light.h"

struct DeletionQueue {
  std::deque<std::function<void()>> deletors;

  void pushFunction(std::function<void()>&& deletor) {
    deletors.push_back(std::move(deletor));
  }

  void flush() {
    // reverse iterate the deletion queue to execute all the functions
    for (auto it = deletors.rbegin(); it != deletors.rend(); ++it) {
      (*it)();  // call functors
    }

    deletors.clear();
  }
};

struct FrameData {
  VkCommandPool command_pool;
  VkCommandBuffer main_command_buffer;
  VkSemaphore swapchain_semaphore, render_semaphore;
  VkFence render_fence;
  DeletionQueue deletion_queue;
  DescriptorAllocatorGrowable frame_descriptors;
};

constexpr unsigned int FRAME_OVERLAP = 3;

struct ComputePushConstants {
  glm::vec4 data1;
  glm::vec4 data2;
  glm::vec4 data3;
  glm::vec4 data4;
};

struct ComputeEffect {
  const char* name;

  VkPipeline pipeline;
  VkPipelineLayout layout;

  ComputePushConstants data;
};

struct GpuPointLightData {
  vec3 position;
  float radius;
  vec4 color;
  float diffuse;
  float specular;
};

struct GpuSceneData {
  mat4 view;
  mat4 proj;
  mat4 viewproj;
  vec3 camera_position;
  float ambient_light_factor;
  vec3 ambient_light_color;
  int number_of_lights;
  GpuPointLightData point_lights[10];
};

class VulkanEngine;

struct GltfMetallicRoughness {
  MaterialPipeline opaque_pipeline;
  MaterialPipeline transparent_pipeline;
  MaterialPipeline wireframe_pipeline;

  VkDescriptorSetLayout material_layout;

  struct MaterialConstants {
    glm::vec4 color_factors;
    glm::vec4 metal_rough_factors;
    // padding?
    glm::vec4 extra[14];
  };

  struct MaterialResources {
    AllocatedImage color_image;
    VkSampler color_sampler;
    AllocatedImage metal_rough_image;
    VkSampler metal_rough_sampler;
    VkBuffer data_buffer;  // material constants
    uint32_t data_buffer_offset;
  };

  DescriptorWriter writer;

  void buildPipelines(VulkanEngine* engine);
  void clearResources(VkDevice device);

  MaterialInstance writeMaterial(
      VkDevice device, MaterialPass pass, const MaterialResources& resources,
      DescriptorAllocatorGrowable& descriptor_allocator);
};

struct RenderObject {
  uint32_t index_count;
  uint32_t first_index;
  VkBuffer index_buffer;

  MaterialInstance* material;
  bounds bounds;
  glm::mat4 transform;
  VkDeviceAddress vertex_buffer_address;
};

struct DrawContext {
  std::vector<RenderObject> opaque_surfaces;
  std::vector<RenderObject> transparent_surfaces;
};

struct MeshNode final : public Node {
  std::shared_ptr<MeshAsset> mesh;

  void Draw(const glm::mat4& top_matrix, DrawContext& ctx) override;
};

struct EngineStats {
  float frametime;
  int triangle_count;
  int drawcall_count;
  float scene_update_time;
  float mesh_draw_time;
};

struct RendererOptions {
  bool wireframe = false;
  bool frustum = true;
};

/// @brief This is the main Vulkan Engine class
class VulkanEngine {
 public:
  bool is_initialized{false};
  int frame_number{0};
  bool stop_rendering{false};
  VkExtent2D window_extent{2560, 1440};
  EngineStats stats;

  struct SDL_Window* window{nullptr};

  static VulkanEngine& get();

  /// @brief Does most of the engine initialization
  void init();

  // shuts down the engine
  void cleanup();

  // draw loop
  void draw();

  // run main loop
  void run();

  void immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function);
  void drawImgui(VkCommandBuffer cmd, VkImageView target_image_view);
  AllocatedBuffer createBuffer(size_t alloc_size, VkBufferUsageFlags usage,
                               VmaMemoryUsage memory_usage);
  void destroyBuffer(const AllocatedBuffer& buffer);
  AllocatedImage createImage(VkExtent3D size, VkFormat format,
                             VkImageUsageFlags usage, bool mipmapped = false);
  AllocatedImage createImage(void* data, VkExtent3D size, VkFormat format,
                             VkImageUsageFlags usage, bool mipmapped = false);
  void destroyImage(const AllocatedImage& img);

  /// @brief Uploads mesh data to gpu buffers
  /// @param indices list of indices in uint32_t format
  /// @param vertices list of vertices
  /// @return returns a struct containing the allocated index and vertex buffer
  /// and its corresponding gpu address
  GPUMeshBuffers uploadMesh(std::span<uint32_t> indices,
                            std::span<Vertex> vertices);

  // vulkan stuff
  VkInstance instance;
  VkDebugUtilsMessengerEXT debug_messenger;
  VkPhysicalDevice chosen_gpu;
  VkDevice device;
  VkSurfaceKHR surface;
  DeletionQueue main_deletion_queue;
  VmaAllocator allocator;

  VkQueue graphics_queue;
  uint32_t graphics_queue_family;

  // Used as the color attachement for actual rendering
  // will be copied into the final swapchain image
  AllocatedImage draw_image;
  AllocatedImage depth_image;
  VkExtent2D draw_extent;
  float render_scale = 1.f;

  //
  // swapchain
  //
  VkSwapchainKHR swapchain;
  VkFormat swapchain_image_format;

  std::vector<VkImage> swapchain_images;
  std::vector<VkImageView> swapchain_image_views;
  VkExtent2D swapchain_extent;

  //
  // command
  //
  FrameData frames[FRAME_OVERLAP];

  FrameData& getCurrentFrame() { return frames[frame_number % FRAME_OVERLAP]; }

  //
  // descriptors
  //
  DescriptorAllocatorGrowable global_descriptor_allocator;

  VkDescriptorSet draw_image_descriptors;
  VkDescriptorSetLayout draw_image_descriptor_layout;

  //
  // Pipeline
  //
  // VkPipeline _gradientPipeline;
  VkPipelineLayout gradient_pipeline_layout;

  //
  // immediate commands
  //
  VkFence imm_fence;
  VkCommandBuffer imm_command_buffer;
  VkCommandPool imm_command_pool;

  std::vector<ComputeEffect> background_effects;
  int current_background_effect{0};

  // ----------
  // scene
  std::vector<PointLight> point_lights;
  GpuSceneData scene_data;
  VkDescriptorSetLayout gpu_scene_data_descriptor_layout;

  AllocatedImage white_image;
  AllocatedImage black_image;
  AllocatedImage grey_image;
  AllocatedImage error_checkerboard_image;

  VkSampler default_sampler_linear;
  VkSampler default_sampler_nearest;

  MaterialInstance default_data;
  GltfMetallicRoughness metal_rough_material;

  DrawContext main_draw_context;

  std::unordered_map<std::string, std::shared_ptr<LoadedGltf>> loaded_scenes;
  Camera3D camera;
  std::unique_ptr<FirstPersonFlyingController> fps_controller;
  CameraController* camera_controller;

  RendererOptions renderer_options;
  
  void resizeSwapchain(uint32_t width, uint32_t height);

 private:
  /// @brief Initializes SDL context and creates SDL window
  void initSdl();

  /// @brief Initializes core vulkan resources
  void initVulkan();

  /// @brief Initializes vulkan device and its queues
  void initDevice();

  /// @brief Initializes VMA
  void initAllocator();

  /// @brief Initializes swapchain.
  /// Creates an intermediate draw image where the actual frame rendering is
  /// done onto. The contents are then blipped to the swap chain image at the
  /// end of the frame. Also creates a depth image.
  void initSwapchain();

  /// @brief Initializes a command pool for each inflight frame of the
  /// swapchain. Also creates an immediate command pool that is used to execute
  /// immediate commands on the gpu. Usefull for ImGui and other generic
  /// purposes.
  void initCommands();

  /// @brief Creates a synchronization structures.
  /// Creates a fence for the immediate command pool.
  /// Creates a fence and two semaphores for each inflight frame of the
  /// swapchain.
  void initSyncStructures();

  /// @brief Creates pipeline descriptors and its allocators.
  /// Creates a growable descriptor allocator for each inflight frame.
  /// Creates a descriptor set for the background compute shader.
  /// Creates a descriptor set for scene data.
  void initDescriptors();

  /// @brief Creates general pipelines.
  /// Creates the background compute pipeline.
  /// Creates the PBR Metalness pipeline.
  void initPipelines();

  /// @brief Creates the background compute pipeline.
  void initBackgroundPipelines();

  /// @brief Initializes ImGui entire context
  void initImgui();

  /// @brief Dispatches compute shader to fill in main image
  /// @param cmd VkCommandBuffer that will queue in the work
  void drawBackground(VkCommandBuffer cmd);

  /// @brief Responsible for queueing commands to render all Renderables.
  /// Sorts based on material and performs frustum culling.
  /// @param cmd VkCommandBuffer that will queue in the work
  void drawGeometry(VkCommandBuffer cmd);

  /// @brief Initializes default white, black, error images and its samplers
  void initDefaultData();
  void initImages();

  void initScene();

  /// @brief Updates scene data and call Draw on each scene node.
  void updateScene();

  void drawSceneHierarchy();

  void createSwapchain(uint32_t width, uint32_t height,
                       VkSwapchainKHR old = VK_NULL_HANDLE);
  void destroySwapchain();
};
