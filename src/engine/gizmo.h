#pragma once

#include "../math/transform.h"
#include "../vk_loader.h"

class GizmoRenderer {
 public:
  void init(VulkanEngine* engine);
  void cleanup();

  void drawBasis(VkCommandBuffer cmd, VkDescriptorSet set,
                    const mat4& world_from_local, const vec4& color);

 private:
  void createPipeline();
  void createDescriptors();
  void loadArrowModel();

  struct GpuGizmoPushConstants {
    mat4 world_from_local;
    VkDeviceAddress vertex_buffer_address;
  };

  struct GpuGizmoData {
    mat4 viewproj_from_model;
    vec4 color;
  };

  VkPipelineLayout layout;
  VkPipeline pipeline;
  VkDescriptorSetLayout descriptor_layout;

  MeshAsset cube;
  VulkanEngine* engine;
};