#pragma once

#include <vk_types.h>

class GfxDevice {
 private:
  VkInstance instance;
  VkPhysicalDevice chosen_gpu;
  VkDevice device;

  VkDebugUtilsMessengerEXT debug_messenger;

  VkSurfaceKHR surface;

  VkQueue graphics_queue;
  uint32_t graphics_queue_family;
};