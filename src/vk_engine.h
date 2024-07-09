﻿// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include "SDL_stdinc.h"
#include "vk_descriptors.h"
#include "vk_loader.h"
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
  DescriptorAllocatorGrowable _frameDescriptors;
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

struct GPUSceneData {
  glm::mat4 view;
  glm::mat4 proj;
  glm::mat4 viewproj;
  glm::vec4 ambientColor;
  glm::vec4 sunlightDirection; // w for sun power
  glm::vec4 sunlightColor;
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
  GPUMeshBuffers uploadMesh(std::span<uint32_t> indices,
                            std::span<Vertex> vertices);

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
  // Mesh Pipeline
  //
  VkPipelineLayout _meshPipelineLayout;
  VkPipeline _meshPipeline;
  std::vector<std::shared_ptr<MeshAsset>> testMeshes;

  //
  // scene
  //
  GPUSceneData sceneData;
  VkDescriptorSetLayout _gpuSceneDataDescriptorLayout;

  AllocatedImage _whiteImage;
  AllocatedImage _blackImage;
  AllocatedImage _greyImage;
  AllocatedImage _errorCheckerboardImage;

  VkSampler _defaultSamplerLinear;
  VkSampler _defaultSamplerNearest;

  // describes a descriptor set with only 1 texture input
  VkDescriptorSetLayout _singleImageDescriptorLayout;

private:
  void init_vulkan();
  void init_swapchain();
  void init_commands();
  void init_sync_structures();
  void init_descriptors();
  void init_pipelines();
  void init_background_pipelines();
  void init_imgui();
  void init_mesh_pipeline();

  void draw_background(VkCommandBuffer cmd);
  void draw_geometry(VkCommandBuffer cmd);
  void init_default_data();
  void init_images();

  //
  // resources
  //
  AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage,
                                VmaMemoryUsage memoryUsage);
  void destroy_buffer(const AllocatedBuffer &buffer);
  AllocatedImage create_image(VkExtent3D size, VkFormat format,
                              VkImageUsageFlags usage, bool mipmapped = false);
  AllocatedImage create_image(void *data, VkExtent3D size, VkFormat format,
                              VkImageUsageFlags usage, bool mipmapped = false);
  void destroy_image(const AllocatedImage &img);

  void create_swapchain(uint32_t width, uint32_t height,
                        VkSwapchainKHR old = VK_NULL_HANDLE);
  void resize_swapchain(uint32_t width, uint32_t height);
  void destroy_swapchain();
};
