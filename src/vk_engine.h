// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include "SDL_stdinc.h"
#include "vk_descriptors.h"
#include <cstdint>
#include <span>
#include <vk_types.h>
#include <vulkan/vulkan_core.h>

#include <deque>
#include <functional>

struct DeletionQueue {
  std::deque<std::function<void()>> deletors;

  void push_function(std::function<void()> &&deletor) {
    deletors.push_back(deletor);
  }

  void flush() {
    // reverse iterate the deletion queue to execute all the functions
    for (auto it = deletors.rbegin(); it != deletors.rend(); it++) {
      (*it)(); // call functors
    }

    deletors.clear();
  }
};

struct FrameData {
  VkCommandPool _commandPool;
  VkCommandBuffer _mainCommandBuffer;
  VkSemaphore _swapchainSemaphore, _renderSemaphore;
  VkFence _renderFence;
  DeletionQueue _deletionQueue;
};
constexpr unsigned int FRAME_OVERLAP = 2;

struct ComputePushConstants {
  glm::vec4 data1;
  glm::vec4 data2;
  glm::vec4 data3;
  glm::vec4 data4;
};

struct ComputeEffect {
  const char *name;

  VkPipeline pipeline;
  VkPipelineLayout layout;

  ComputePushConstants data;
};

class VulkanEngine {
public:
  bool _isInitialized{false};
  int _frameNumber{0};
  bool stop_rendering{false};
  VkExtent2D _windowExtent{1700, 900};

  struct SDL_Window *_window{nullptr};

  static VulkanEngine &Get();

  // initializes everything in the engine
  void init();

  // shuts down the engine
  void cleanup();

  // draw loop
  void draw();

  // run main loop
  void run();

  void immediateSubmit(std::function<void(VkCommandBuffer cmd)> &&function);
  void drawImgui(VkCommandBuffer cmd, VkImageView targetImageView);
  
  // mesh
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

  //
  AllocatedImage _drawImage;
  VkExtent2D _drawExtent;

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
  FrameData &get_current_frame() {
    return _frames[_frameNumber % FRAME_OVERLAP];
  }

  //
  // descriptors
  //
  DescriptorAllocator globalDescriptorAllocator;

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
  // triangle pipeline
  //
  VkPipelineLayout _trianglePipelineLayout;
  VkPipeline _trianglePipeline;

  //
  // Mesh Pipeline
  //
  VkPipelineLayout _meshPipelineLayout;
  VkPipeline _meshPipeline;
  GPUMeshBuffers rectangle;

private:
  void init_vulkan();
  void init_swapchain();
  void init_commands();
  void init_sync_structures();
  void init_descriptors();
  void init_pipelines();
  void init_background_pipelines();
  void init_imgui();
  void init_triangle_pipeline();
  void init_mesh_pipeline();

  void draw_background(VkCommandBuffer cmd);
  void draw_geometry(VkCommandBuffer cmd);
  void init_default_data();

  AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
  void destroy_buffer(const AllocatedBuffer& buffer);

  void create_swapchain(uint32_t width, uint32_t height);
  void destroy_swapchain();
};
