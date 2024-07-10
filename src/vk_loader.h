#pragma once

#include "fastgltf/types.hpp"
#include "vk_descriptors.h"
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <vk_types.h>
#include <vulkan/vulkan_core.h>

struct GLTFMaterial {
  MaterialInstance data;
};

struct Bounds {
    glm::vec3 origin;
    float sphereRadius;
    glm::vec3 extents;
};

struct GeoSurface {
  uint32_t startIndex;
  uint32_t count;
  Bounds bounds;
  std::shared_ptr<GLTFMaterial> material;
};

struct MeshAsset {
  std::string name;
  std::vector<GeoSurface> surfaces;
  GPUMeshBuffers meshBuffers;
};

class VulkanEngine;

struct LoadedGLTF : public IRenderable {
  using MeshMap = std::unordered_map<std::string, std::shared_ptr<MeshAsset>>;
  using NodeMap = std::unordered_map<std::string, std::shared_ptr<Node>>;
  using ImageMap = std::unordered_map<std::string, AllocatedImage>;
  using MaterialMap =
      std::unordered_map<std::string, std::shared_ptr<GLTFMaterial>>;

  MeshMap meshes;
  NodeMap nodes;
  ImageMap images;
  MaterialMap materials;

  // root nodes with no parents
  std::vector<std::shared_ptr<Node>> topNodes;
  std::vector<VkSampler> samplers;

  DescriptorAllocatorGrowable descriptorPool;
  AllocatedBuffer materialDataBuffer;

  VulkanEngine *creator;
  ~LoadedGLTF() { clearAll(); }

  virtual void Draw(const glm::mat4 &topMatrix, DrawContext &ctx) override;

private:
  void clearAll();
};

std::optional<std::shared_ptr<LoadedGLTF>> loadGltf(VulkanEngine *engine,
                                                    std::string_view filePath);

std::optional<std::vector<std::shared_ptr<MeshAsset>>>
loadGltfMeshes(VulkanEngine *engine, std::filesystem::path filePath);
std::optional<AllocatedImage> load_image(VulkanEngine* engine, fastgltf::Asset& asset, fastgltf::Image& image);