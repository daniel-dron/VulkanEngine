﻿// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include "vk_descriptors.h"
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

  // vulkan stuff
  VkInstance _instance;
  VkDebugUtilsMessengerEXT _debug_messenger;
  VkPhysicalDevice _chosenGPU;
  VkDevice _device;
  VkSurfaceKHR _surface;
  DeletionQueue _mainDeletionQueue;
  VmaAllocator _allocator;

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
  VkPipeline _gradientPipeline;
  VkPipelineLayout _gradientPipelineLayout;

  VkQueue _graphicsQueue;
  uint32_t _graphicsQueueFamily;

private:
  void init_vulkan();
  void init_swapchain();
  void init_commands();
  void init_sync_structures();
  void init_descriptors();
  void init_pipelines();
  void init_background_pipelines();

  void draw_background(VkCommandBuffer cmd);

  void create_swapchain(uint32_t width, uint32_t height);
  void destroy_swapchain();
};