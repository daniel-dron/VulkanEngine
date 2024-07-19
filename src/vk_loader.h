#pragma once

#include <vk_types.h>
#include <vulkan/vulkan_core.h>

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "fastgltf/types.hpp"
#include "vk_descriptors.h"

struct GltfMaterial {
  std::string name;
  MaterialInstance data;
};

struct bounds {
  glm::vec3 origin;
  float sphereRadius;
  glm::vec3 extents;
};

struct geo_surface {
  uint32_t start_index;
  uint32_t count;
  bounds bounds;
  std::shared_ptr<GltfMaterial> material;
};

struct mesh_asset {
  std::string name;
  std::vector<geo_surface> surfaces;
  GPUMeshBuffers mesh_buffers;
};

class VulkanEngine;

struct LoadedGltf final : public IRenderable {
  using mesh_map = std::unordered_map<std::string, std::shared_ptr<mesh_asset>>;
  using node_map = std::unordered_map<std::string, std::shared_ptr<Node>>;
  using image_map = std::unordered_map<std::string, AllocatedImage>;
  using material_map =
      std::unordered_map<std::string, std::shared_ptr<GltfMaterial>>;

  mesh_map meshes;
  node_map nodes;
  image_map images;
  material_map materials;

  // root nodes with no parents
  std::vector<std::shared_ptr<Node>> top_nodes;
  std::vector<VkSampler> samplers;

  DescriptorAllocatorGrowable descriptor_pool;
  AllocatedBuffer material_data_buffer;

  VulkanEngine* creator;
  ~LoadedGltf() override { clear_all(); }

  void Draw(const glm::mat4& top_matrix, DrawContext& ctx) override;

 private:
  void clear_all();
};

std::optional<std::shared_ptr<LoadedGltf>> load_gltf(
    VulkanEngine* engine, std::string_view filePath);

std::optional<std::pair<AllocatedImage, std::string>> load_image(
    VulkanEngine* engine, fastgltf::Asset& asset, fastgltf::Image& image);
