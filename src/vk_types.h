// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.
#pragma once

#include "glm/ext/vector_float4.hpp"
#include <array>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <vk_mem_alloc.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan.h>

#include <fmt/core.h>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <vulkan/vulkan_core.h>

#define VK_CHECK(x)                                                            \
  do {                                                                         \
    VkResult err = x;                                                          \
    if (err) {                                                                 \
      fmt::println("{} {} Detected Vulkan error: {}", __FILE__, __LINE__,      \
                   string_VkResult(err));                                      \
      abort();                                                                 \
    }                                                                          \
  } while (0)

struct AllocatedImage {
  VkImage image;
  VkImageView view;
  VmaAllocation allocation;
  VkExtent3D extent;
  VkFormat format;
};

struct AllocatedBuffer {
  VkBuffer buffer;
  VmaAllocation allocation;
  VmaAllocationInfo info;
};

struct Vertex {
  glm::vec3 position;
  float uv_x;
  glm::vec3 normal;
  float uv_y;
  glm::vec4 color;
};

// resources needed for a single mesh
struct GPUMeshBuffers {
  AllocatedBuffer indexBuffer;
  AllocatedBuffer vertexBuffer;
  VkDeviceAddress vertexBufferAddress;
};

// push constants for our mesh object to be drawn
struct GPUDrawPushConstants {
  glm::mat4 worldMatrix;
  VkDeviceAddress vertexBuffer;
};

//
// Rendering system types
//

enum class MaterialPass : uint8_t { MainColor, Transparent, Other };

struct MaterialPipeline {
  VkPipeline pipeline;
  VkPipelineLayout layout;
};

struct MaterialInstance {
  MaterialPipeline *pipeline;
  MaterialPass passType;
  VkDescriptorSet materialSet;
};