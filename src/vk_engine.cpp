//> includes
#include "vk_engine.h"
#include "SDL_events.h"
#include "SDL_video.h"
#include "fmt/core.h"
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include <cassert>
#include <cstring>
#include <vk_images.h>
#include <vk_initializers.h>
#include <vk_pipelines.h>
#include <vk_types.h>

#include <VkBootstrap.h>

#include <chrono>
#include <thread>
#include <vulkan/vulkan_core.h>

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

VulkanEngine *loadedEngine = nullptr;
constexpr bool bUseValidationLayers = true;

VulkanEngine &VulkanEngine::Get() { return *loadedEngine; }
void VulkanEngine::init() {
  // only one engine initialization is allowed with the application.
  assert(loadedEngine == nullptr);
  loadedEngine = this;

  // We initialize SDL and create a window with it.
  SDL_Init(SDL_INIT_VIDEO);

  SDL_WindowFlags window_flags =
      (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

  _window = SDL_CreateWindow("Vulkan Engine", SDL_WINDOWPOS_UNDEFINED,
                             SDL_WINDOWPOS_UNDEFINED, _windowExtent.width,
                             _windowExtent.height, window_flags);

  init_vulkan();

  init_swapchain();

  init_commands();

  init_sync_structures();

  init_descriptors();

  init_pipelines();

  init_default_data();

  init_imgui();

  // everything went fine
  _isInitialized = true;
}

void VulkanEngine::init_vulkan() {
  vkb::InstanceBuilder builder;

  // basic debug features
  auto inst_ret = builder.set_app_name("Example Vulkan App")
                      .request_validation_layers(bUseValidationLayers)
                      .use_default_debug_messenger()
                      .require_api_version(1, 3, 0)
                      .build();

  vkb::Instance vkb_inst = inst_ret.value();

  _instance = vkb_inst.instance;
  _debug_messenger = vkb_inst.debug_messenger;

  //
  // device
  //
  SDL_Vulkan_CreateSurface(_window, _instance, &_surface);

  // 1.3 features
  VkPhysicalDeviceVulkan13Features features{
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
  features.dynamicRendering = true;
  features.synchronization2 = true;

  // 1.2 features
  VkPhysicalDeviceVulkan12Features features12{
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
  features12.bufferDeviceAddress = true;
  features12.descriptorIndexing = true;

  // select gpu than can write to SDL surface and supports 1.3
  vkb::PhysicalDeviceSelector selector{vkb_inst};
  vkb::PhysicalDevice physicalDevice = selector.set_minimum_version(1, 3)
                                           .set_required_features_13(features)
                                           .set_required_features_12(features12)
                                           .set_surface(_surface)
                                           .select()
                                           .value();

  vkb::DeviceBuilder deviceBuilder{physicalDevice};

  vkb::Device vkbDevice = deviceBuilder.build().value();

  _device = vkbDevice.device;
  _chosenGPU = physicalDevice.physical_device;

  //
  // queue
  //
  _graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
  _graphicsQueueFamily =
      vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

  //
  // memory allocator
  //
  VmaAllocatorCreateInfo allocatorInfo = {};
  allocatorInfo.device = _device;
  allocatorInfo.physicalDevice = _chosenGPU;
  allocatorInfo.instance = _instance;
  allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
  vmaCreateAllocator(&allocatorInfo, &_allocator);

  _mainDeletionQueue.push_function([&]() { vmaDestroyAllocator(_allocator); });
}

void VulkanEngine::init_swapchain() {
  create_swapchain(_windowExtent.width, _windowExtent.height);

  VkExtent3D drawImageExtent = {
      .width = _windowExtent.width, .height = _windowExtent.height, .depth = 1};

  _drawImage.extent = drawImageExtent;
  _drawImage.format = VK_FORMAT_R16G16B16A16_SFLOAT;

  VkImageUsageFlags drawImageUsages{};
  drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
  drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  VkImageCreateInfo rimg_info = vkinit::image_create_info(
      _drawImage.format, drawImageUsages, drawImageExtent);

  // for the draw image, we want to allocate it from gpu local memory
  VmaAllocationCreateInfo rimg_allocinfo = {};
  rimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
  rimg_allocinfo.requiredFlags =
      VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  vmaCreateImage(_allocator, &rimg_info, &rimg_allocinfo, &_drawImage.image,
                 &_drawImage.allocation, nullptr);

  auto rview_info = vkinit::imageview_create_info(
      _drawImage.format, _drawImage.image, VK_IMAGE_ASPECT_COLOR_BIT);

  VK_CHECK(vkCreateImageView(_device, &rview_info, nullptr, &_drawImage.view));

  _depthImage.format = VK_FORMAT_D32_SFLOAT;
  _depthImage.extent = drawImageExtent;
  VkImageUsageFlags depthImageUsages{};
  depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

  VkImageCreateInfo dimg_info = vkinit::image_create_info(
      _depthImage.format, depthImageUsages, drawImageExtent);

  // allocate and create the image
  vmaCreateImage(_allocator, &dimg_info, &rimg_allocinfo, &_depthImage.image,
                 &_depthImage.allocation, nullptr);

  // build a image-view for the draw image to use for rendering
  VkImageViewCreateInfo dview_info = vkinit::imageview_create_info(
      _depthImage.format, _depthImage.image, VK_IMAGE_ASPECT_DEPTH_BIT);

  VK_CHECK(vkCreateImageView(_device, &dview_info, nullptr, &_depthImage.view));

  // add to deletion queues
  _mainDeletionQueue.push_function([&]() {
    vkDestroyImageView(_device, _drawImage.view, nullptr);
    vmaDestroyImage(_allocator, _drawImage.image, _drawImage.allocation);

    vkDestroyImageView(_device, _depthImage.view, nullptr);
    vmaDestroyImage(_allocator, _depthImage.image, _depthImage.allocation);
  });
}

void VulkanEngine::init_commands() {
  VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(
      _graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

  for (int i = 0; i < FRAME_OVERLAP; i++) {
    VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr,
                                 &_frames[i]._commandPool));

    // default command buffer that will be used for rendering
    VkCommandBufferAllocateInfo cmdAllocInfo =
        vkinit::command_buffer_allocate_info(_frames[i]._commandPool, 1);
    VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo,
                                      &_frames[i]._mainCommandBuffer));
  }

  //
  // imgui commands
  //
  VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr,
                               &_immCommandPool));

  auto cmdAllocInfo = vkinit::command_buffer_allocate_info(_immCommandPool, 1);
  VK_CHECK(
      vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_immCommandBuffer));

  _mainDeletionQueue.push_function(
      [&]() { vkDestroyCommandPool(_device, _immCommandPool, nullptr); });
}

void VulkanEngine::init_sync_structures() {
  // one fence to control when the gpu has finished rendering the frame
  // 2 semaphores to sync rendering with swapchan

  // the fence starts signalled so we can wait on it on the first frame
  // otherwise, if we called wait on it without being signaled, the cpu would
  // wait forever, as the gpu hasnt yet started doing work
  auto fenceCreateInfo =
      vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
  auto semaphoreCreateInfo = vkinit::semaphore_create_info();

  for (int i = 0; i < FRAME_OVERLAP; i++) {
    VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr,
                           &_frames[i]._renderFence));

    VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr,
                               &_frames[i]._renderSemaphore));
    VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr,
                               &_frames[i]._swapchainSemaphore));
  }

  //
  // immediate fence
  //
  VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_immFence));
  _mainDeletionQueue.push_function(
      [&]() { vkDestroyFence(_device, _immFence, nullptr); });
}

void VulkanEngine::init_descriptors() {
  std::vector<DescriptorAllocator::PoolSizeRatio> sizes = {
      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1}};

  globalDescriptorAllocator.init_pool(_device, 10, sizes);

  {
    DescriptorLayoutBuilder builder;
    builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    _drawImageDescriptorLayout =
        builder.build(_device, VK_SHADER_STAGE_COMPUTE_BIT, nullptr);
  }

  _drawImageDescriptors =
      globalDescriptorAllocator.allocate(_device, _drawImageDescriptorLayout);

  VkDescriptorImageInfo imgInfo{};
  imgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
  imgInfo.imageView = _drawImage.view;

  VkWriteDescriptorSet drawImageWrite = {};
  drawImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  drawImageWrite.pNext = nullptr;

  drawImageWrite.dstBinding = 0;
  drawImageWrite.dstSet = _drawImageDescriptors;
  drawImageWrite.descriptorCount = 1;
  drawImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  drawImageWrite.pImageInfo = &imgInfo;

  vkUpdateDescriptorSets(_device, 1, &drawImageWrite, 0, nullptr);

  // make sure both the descriptor allocator and the new layout get cleaned up
  // properly
  _mainDeletionQueue.push_function([&]() {
    globalDescriptorAllocator.destroy_pool(_device);

    vkDestroyDescriptorSetLayout(_device, _drawImageDescriptorLayout, nullptr);
  });
}

void VulkanEngine::init_pipelines() {
  // compute
  init_background_pipelines();

  // graphics
  init_mesh_pipeline();
}

void VulkanEngine::init_background_pipelines() {
  VkPipelineLayoutCreateInfo computeLayout{};
  computeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  computeLayout.pNext = nullptr;
  computeLayout.pSetLayouts = &_drawImageDescriptorLayout;
  computeLayout.setLayoutCount = 1;

  // push constants
  VkPushConstantRange pushConstant{};
  pushConstant.offset = 0;
  pushConstant.size = sizeof(ComputePushConstants);
  pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  computeLayout.pushConstantRangeCount = 1;
  computeLayout.pPushConstantRanges = &pushConstant;

  VK_CHECK(vkCreatePipelineLayout(_device, &computeLayout, nullptr,
                                  &_gradientPipelineLayout));

  VkShaderModule gradientShader;
  if (!vkutil::load_shader_module("../shaders/gradient_color.comp.spv", _device,
                                  &gradientShader)) {
    fmt::println(
        "Error when building the compute shader [gradient_color.comp]");
  }

  VkShaderModule skyShader;
  if (!vkutil::load_shader_module("../shaders/sky.comp.spv", _device,
                                  &skyShader)) {
    fmt::println("Error when building the compute shader [sky.comp]");
  }

  VkPipelineShaderStageCreateInfo stageinfo{};
  stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stageinfo.pNext = nullptr;
  stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  stageinfo.module = gradientShader;
  stageinfo.pName = "main";

  VkComputePipelineCreateInfo computePipelineCreateInfo{};
  computePipelineCreateInfo.sType =
      VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  computePipelineCreateInfo.pNext = nullptr;
  computePipelineCreateInfo.layout = _gradientPipelineLayout;
  computePipelineCreateInfo.stage = stageinfo;

  ComputeEffect gradient;
  gradient.layout = _gradientPipelineLayout;
  gradient.name = "gradient";
  gradient.data = {.data1 = glm::vec4(1, 0, 0, 1),
                   .data2 = glm::vec4(0, 0, 1, 1)};

  VK_CHECK(vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1,
                                    &computePipelineCreateInfo, nullptr,
                                    &gradient.pipeline));

  ComputeEffect sky;
  sky.layout = _gradientPipelineLayout;
  sky.name = "sky";
  sky.data = {.data1 = glm::vec4(0.1, 0.2, 0.4, 0.97)};

  VK_CHECK(vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1,
                                    &computePipelineCreateInfo, nullptr,
                                    &sky.pipeline));

  backgroundEffects.push_back(gradient);
  backgroundEffects.push_back(sky);

  vkDestroyShaderModule(_device, gradientShader, nullptr);
  vkDestroyShaderModule(_device, skyShader, nullptr);
  _mainDeletionQueue.push_function([&, sky, gradient]() {
    vkDestroyPipelineLayout(_device, _gradientPipelineLayout, nullptr);
    vkDestroyPipeline(_device, sky.pipeline, nullptr);
    vkDestroyPipeline(_device, gradient.pipeline, nullptr);
  });
}

void VulkanEngine::init_imgui() {
  // 1: create descriptor pool for IMGUI
  //  the size of the pool is very oversize, but it's copied from imgui demo
  //  itself.
  VkDescriptorPoolSize pool_sizes[] = {
      {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
      {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
      {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}};

  VkDescriptorPoolCreateInfo pool_info = {};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  pool_info.maxSets = 1000;
  pool_info.poolSizeCount = (uint32_t)std::size(pool_sizes);
  pool_info.pPoolSizes = pool_sizes;

  VkDescriptorPool imguiPool;
  VK_CHECK(vkCreateDescriptorPool(_device, &pool_info, nullptr, &imguiPool));

  // 2: initialize imgui library

  // this initializes the core structures of imgui
  ImGui::CreateContext();

  // this initializes imgui for SDL
  ImGui_ImplSDL2_InitForVulkan(_window);

  // this initializes imgui for Vulkan
  ImGui_ImplVulkan_InitInfo init_info = {};
  init_info.Instance = _instance;
  init_info.PhysicalDevice = _chosenGPU;
  init_info.Device = _device;
  init_info.Queue = _graphicsQueue;
  init_info.DescriptorPool = imguiPool;
  init_info.MinImageCount = 3;
  init_info.ImageCount = 3;
  init_info.UseDynamicRendering = true;

  // dynamic rendering parameters for imgui to use
  init_info.PipelineRenderingCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
  init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
  init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats =
      &_swapchainImageFormat;

  init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

  ImGui_ImplVulkan_Init(&init_info);

  ImGui_ImplVulkan_CreateFontsTexture();

  // add the destroy the imgui created structures
  _mainDeletionQueue.push_function([&, imguiPool]() {
    ImGui_ImplVulkan_Shutdown();
    vkDestroyDescriptorPool(_device, imguiPool, nullptr);
  });
}

void VulkanEngine::init_mesh_pipeline() {
  VkShaderModule triangleFragShader;
  if (!vkutil::load_shader_module("../shaders/colored_triangle.frag.spv",
                                  _device, &triangleFragShader)) {
    fmt::println("Error when building the triangle fragment shader module");
  } else {
    fmt::println("Triangle fragment shader succesfully loaded");
  }

  VkShaderModule triangleVertexShader;
  if (!vkutil::load_shader_module("../shaders/colored_triangle_mesh.vert.spv",
                                  _device, &triangleVertexShader)) {
    fmt::println("Error when building the triangle vertex shader module");
  } else {
    fmt::println("Triangle vertex shader succesfully loaded");
  }

  VkPushConstantRange bufferRange{};
  bufferRange.offset = 0;
  bufferRange.size = sizeof(GPUDrawPushConstants);
  bufferRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

  VkPipelineLayoutCreateInfo pipeline_layout_info =
      vkinit::pipeline_layout_create_info();
  pipeline_layout_info.pPushConstantRanges = &bufferRange;
  pipeline_layout_info.pushConstantRangeCount = 1;

  VK_CHECK(vkCreatePipelineLayout(_device, &pipeline_layout_info, nullptr,
                                  &_meshPipelineLayout));

  vkutil::PipelineBuilder pipelineBuilder;
  // use the triangle layout we created
  pipelineBuilder._pipelineLayout = _meshPipelineLayout;
  // connecting the vertex and pixel shaders to the pipeline
  pipelineBuilder.set_shaders(triangleVertexShader, triangleFragShader);
  // it will draw triangles
  pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
  // filled triangles
  pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
  // no backface culling
  pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
  // no multisampling
  pipelineBuilder.set_multisampling_none();
  // blending
  pipelineBuilder.enable_blending_alphablend();
  // depth testing
  pipelineBuilder.enable_depthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);

  // connect the image format we will draw into, from draw image
  pipelineBuilder.set_color_attachment_format(_drawImage.format);
  pipelineBuilder.set_depth_format(_depthImage.format);

  // finally build the pipeline
  _meshPipeline = pipelineBuilder.build_pipeline(_device);

  // clean structures
  vkDestroyShaderModule(_device, triangleFragShader, nullptr);
  vkDestroyShaderModule(_device, triangleVertexShader, nullptr);

  _mainDeletionQueue.push_function([&]() {
    vkDestroyPipelineLayout(_device, _meshPipelineLayout, nullptr);
    vkDestroyPipeline(_device, _meshPipeline, nullptr);
  });
}

AllocatedBuffer VulkanEngine::create_buffer(size_t allocSize,
                                            VkBufferUsageFlags usage,
                                            VmaMemoryUsage memoryUsage) {

  VkBufferCreateInfo bufferInfo = {.sType =
                                       VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  bufferInfo.pNext = nullptr;
  bufferInfo.size = allocSize;

  bufferInfo.usage = usage;

  VmaAllocationCreateInfo vmaallocInfo = {};
  vmaallocInfo.usage = memoryUsage;
  vmaallocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

  AllocatedBuffer newBuffer;
  VK_CHECK(vmaCreateBuffer(_allocator, &bufferInfo, &vmaallocInfo,
                           &newBuffer.buffer, &newBuffer.allocation,
                           &newBuffer.info));

  return newBuffer;
}

void VulkanEngine::destroy_buffer(const AllocatedBuffer &buffer) {
  vmaDestroyBuffer(_allocator, buffer.buffer, buffer.allocation);
}

GPUMeshBuffers VulkanEngine::uploadMesh(std::span<uint32_t> indices,
                                        std::span<Vertex> vertices) {

  const size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
  const size_t indexBufferSize = indices.size() * sizeof(uint32_t);

  GPUMeshBuffers newSurface;

  newSurface.vertexBuffer = create_buffer(
      vertexBufferSize,
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
      VMA_MEMORY_USAGE_GPU_ONLY);

  // find the adress of the vertex buffer
  VkBufferDeviceAddressInfo deviceAddressInfo{
      .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
      .buffer = newSurface.vertexBuffer.buffer};
  newSurface.vertexBufferAddress =
      vkGetBufferDeviceAddress(_device, &deviceAddressInfo);

  newSurface.indexBuffer = create_buffer(indexBufferSize,
                                         VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                                             VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                         VMA_MEMORY_USAGE_GPU_ONLY);

  //
  // staging phase.
  // create a temp buffer used to write the data from the cpu
  // then issue a gpu command to copy that data to the gpu only buffer
  //
  AllocatedBuffer staging = create_buffer(vertexBufferSize + indexBufferSize,
                                          VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                          VMA_MEMORY_USAGE_CPU_ONLY);

  // copy memory to gpu
  void *data = staging.allocation->GetMappedData();
  memcpy(data, vertices.data(), vertexBufferSize);
  memcpy((char *)data + vertexBufferSize, indices.data(), indexBufferSize);

  immediateSubmit([&](VkCommandBuffer cmd) {
    VkBufferCopy vertexCopy{0};
    vertexCopy.dstOffset = 0;
    vertexCopy.srcOffset = 0;
    vertexCopy.size = vertexBufferSize;
    vkCmdCopyBuffer(cmd, staging.buffer, newSurface.vertexBuffer.buffer, 1,
                    &vertexCopy);

    VkBufferCopy indexCopy{0};
    indexCopy.dstOffset = 0;
    indexCopy.srcOffset = vertexBufferSize;
    indexCopy.size = indexBufferSize;
    vkCmdCopyBuffer(cmd, staging.buffer, newSurface.indexBuffer.buffer, 1,
                    &indexCopy);
  });

  destroy_buffer(staging);

  return newSurface;
}

void VulkanEngine::create_swapchain(uint32_t width, uint32_t height, VkSwapchainKHR old) {
  vkb::SwapchainBuilder swapchainBuilder{_chosenGPU, _device, _surface};

  _swapchainImageFormat = VK_FORMAT_B8G8R8A8_SRGB;

  vkb::Swapchain vkbSwapchain =
      swapchainBuilder
          .set_desired_format(VkSurfaceFormatKHR{
              .format = _swapchainImageFormat,
              .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
          .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
          .set_desired_extent(width, height)
          .set_old_swapchain(old)
          .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
          .build()
          .value();

  _swapchain = vkbSwapchain.swapchain;
  _swapchainImages = vkbSwapchain.get_images().value();
  _swapchainImageViews = vkbSwapchain.get_image_views().value();
  _swapchainExtent = {.width = width, .height = height};
}

void VulkanEngine::resize_swapchain(uint32_t width, uint32_t height) {
  vkDeviceWaitIdle(_device);

  _windowExtent.width = width;
  _windowExtent.height = height;

  // save to delete them after creating the swap chain
  auto old = _swapchain;
  auto oldImagesViews = _swapchainImageViews;

  create_swapchain(_windowExtent.width, _windowExtent.height, _swapchain);
  
  // recreate the swapchain semaphore since it has already been signaled by the vkAcquireNextImageKHR
  vkDestroySemaphore(_device, get_current_frame()._swapchainSemaphore, nullptr);
  auto semaphoreCreateInfo = vkinit::semaphore_create_info();
  VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr,
                               &get_current_frame()._swapchainSemaphore));

  vkDestroySwapchainKHR(_device, old, nullptr);
  for (unsigned int i = 0; i < oldImagesViews.size(); i++) {
    vkDestroyImageView(_device, oldImagesViews.at(i), nullptr);
  }
}

void VulkanEngine::destroy_swapchain() {
  vkDestroySwapchainKHR(_device, _swapchain, nullptr);

  for (unsigned int i = 0; i < _swapchainImageViews.size(); i++) {
    vkDestroyImageView(_device, _swapchainImageViews.at(i), nullptr);
  }
}

void VulkanEngine::cleanup() {
  if (_isInitialized) {
    // wait for gpu work to finish
    vkDeviceWaitIdle(_device);

    for (int i = 0; i < FRAME_OVERLAP; i++) {
      vkDestroyCommandPool(_device, _frames[i]._commandPool, nullptr);

      // sync objects
      vkDestroyFence(_device, _frames[i]._renderFence, nullptr);
      vkDestroySemaphore(_device, _frames[i]._renderSemaphore, nullptr);
      vkDestroySemaphore(_device, _frames[i]._swapchainSemaphore, nullptr);

      _frames[i]._deletionQueue.flush();
    }

    _mainDeletionQueue.push_function([&]() {
      for (auto &mesh : testMeshes) {
        destroy_buffer(mesh->meshBuffers.indexBuffer);
        destroy_buffer(mesh->meshBuffers.vertexBuffer);
      }
    });

    _mainDeletionQueue.flush();

    destroy_swapchain();

    vkDestroySurfaceKHR(_instance, _surface, nullptr);
    vkDestroyDevice(_device, nullptr);

    vkb::destroy_debug_utils_messenger(_instance, _debug_messenger);
    vkDestroyInstance(_instance, nullptr);

    SDL_DestroyWindow(_window);
  }

  // clear engine pointer
  loadedEngine = nullptr;
}

void VulkanEngine::draw() {
  // wait for last frame rendering phase. 1 sec timeout
  VK_CHECK(vkWaitForFences(_device, 1, &get_current_frame()._renderFence, true,
                           1000000000));

  get_current_frame()._deletionQueue.flush();

  uint32_t swapchainImageIndex;
  VkResult e = (vkAcquireNextImageKHR(_device, _swapchain, 1000000000,
                                      get_current_frame()._swapchainSemaphore,
                                      0, &swapchainImageIndex));
  if (e != VK_SUCCESS) {
    return;
  }

  // Reset after acquire the next image from the swapchain
  // Incase of error, this fence would never get passed to the queue, thus never
  // triggering leaving us with a timeout next time we wait for fence
  VK_CHECK(vkResetFences(_device, 1, &get_current_frame()._renderFence));

  _drawExtent.height =
      std::min(_swapchainExtent.height, _drawImage.extent.height) * renderScale;
  _drawExtent.width =
      std::min(_swapchainExtent.width, _drawImage.extent.width) * renderScale;

  //
  // commands
  //
  auto cmd = get_current_frame()._mainCommandBuffer;

  // we can safely reset the command buffer because we waited for the render
  // fence
  VK_CHECK(vkResetCommandBuffer(cmd, 0));

  // begin recording. will only use this command buffer once
  auto cmdBeginInfo = vkinit::command_buffer_begin_info(
      VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
  VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

  // transition our main draw image into general layout so it can
  // be written into
  vkutil::transition_image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED,
                           VK_IMAGE_LAYOUT_GENERAL);

  draw_background(cmd);

  vkutil::transition_image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_GENERAL,
                           VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  vkutil::transition_image(cmd, _depthImage.image, VK_IMAGE_LAYOUT_UNDEFINED,
                           VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

  draw_geometry(cmd);

  // transform drawImg into source layout
  // transform swapchain img into dst layout
  vkutil::transition_image(cmd, _drawImage.image,
                           VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  vkutil::transition_image(cmd, _swapchainImages[swapchainImageIndex],
                           VK_IMAGE_LAYOUT_UNDEFINED,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  vkutil::copy_image_to_image(cmd, _drawImage.image,
                              _swapchainImages[swapchainImageIndex],
                              _drawExtent, _swapchainExtent);

  // transition swapchain to present
  vkutil::transition_image(cmd, _swapchainImages[swapchainImageIndex],
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  // draw imgui into the swapchain image
  drawImgui(cmd, _swapchainImageViews[swapchainImageIndex]);

  // set swapchain image layout to Present so we can draw it
  vkutil::transition_image(cmd, _swapchainImages[swapchainImageIndex],
                           VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                           VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

  VK_CHECK(vkEndCommandBuffer(cmd));

  //
  // send commands
  //

  // wait on _swapchainSemaphore. signaled when the swap chain is ready
  // wait on _renderSemaphore. signaled when rendering has finished
  auto cmdinfo = vkinit::command_buffer_submit_info(cmd);

  auto waitInfo = vkinit::semaphore_submit_info(
      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
      get_current_frame()._swapchainSemaphore);
  auto signalInfo =
      vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
                                    get_current_frame()._renderSemaphore);

  auto submit = vkinit::submit_info(&cmdinfo, &signalInfo, &waitInfo);

  // submit command buffer and execute it
  // _renderFence will now block until the commands finish
  VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submit,
                          get_current_frame()._renderFence));

  //
  // present
  //
  VkPresentInfoKHR presentInfo = {};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  presentInfo.pNext = nullptr;
  presentInfo.pSwapchains = &_swapchain;
  presentInfo.swapchainCount = 1;

  // wait on _renderSemaphore, since we need the rendering to have finished
  // to display to the screen
  presentInfo.pWaitSemaphores = &get_current_frame()._renderSemaphore;
  presentInfo.waitSemaphoreCount = 1;

  presentInfo.pImageIndices = &swapchainImageIndex;

  vkQueuePresentKHR(_graphicsQueue, &presentInfo);

  // increase frame number for next loop
  _frameNumber++;
}

void VulkanEngine::draw_background(VkCommandBuffer cmd) {
  // flash color 120 frames period
  VkClearColorValue clearValue;
  float flash = std::abs(std::sin(_frameNumber / 120.0f));
  clearValue = {{0.0f, 0.0f, flash, 1.0f}};

  // what part of the view we want to clear (color bit)
  VkImageSubresourceRange clearRange =
      vkinit::image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);

  // clear image
  // vkCmdClearColorImage(cmd, _drawImage.image, VK_IMAGE_LAYOUT_GENERAL,
  //  &clearValue, 1, &clearRange);

  auto &effect = backgroundEffects[currentBackgroundEffect];

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, effect.pipeline);

  // bind the descriptor set containing the draw image for the compute pipeline
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                          _gradientPipelineLayout, 0, 1, &_drawImageDescriptors,
                          0, nullptr);

  vkCmdPushConstants(cmd, _gradientPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
                     0, sizeof(ComputePushConstants), &effect.data);

  // execute the compute pipeline dispatch. We are using 16x16 workgroup size so
  // we need to divide by it
  vkCmdDispatch(cmd, std::ceil(_drawExtent.width / 16.0),
                std::ceil(_drawExtent.height / 16.0), 1);
}

void VulkanEngine::draw_geometry(VkCommandBuffer cmd) {
  VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(
      _drawImage.view, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(
      _depthImage.view, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

  VkRenderingInfo renderInfo =
      vkinit::rendering_info(_drawExtent, &colorAttachment, &depthAttachment);
  vkCmdBeginRendering(cmd, &renderInfo);

  //
  // mesh
  //
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _meshPipeline);

  VkViewport viewport = {};
  viewport.x = 0;
  viewport.y = 0;
  viewport.width = _drawExtent.width;
  viewport.height = _drawExtent.height;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;

  vkCmdSetViewport(cmd, 0, 1, &viewport);

  VkRect2D scissor = {};
  scissor.offset.x = 0;
  scissor.offset.y = 0;
  scissor.extent.width = _drawExtent.width;
  scissor.extent.height = _drawExtent.height;

  vkCmdSetScissor(cmd, 0, 1, &scissor);

  GPUDrawPushConstants push_constants;
  glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3{0, 0, -5});
  glm::mat4 projection = glm::perspective(
      glm::radians(70.f), (float)_drawExtent.width / (float)_drawExtent.height,
      10000.f, 0.1f);
  projection[1][1] *= -1;
  push_constants.worldMatrix = projection * view;
  // push_constants.vertexBuffer = rectangle.vertexBufferAddress;
  push_constants.vertexBuffer = testMeshes[2]->meshBuffers.vertexBufferAddress;

  vkCmdPushConstants(cmd, _meshPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                     sizeof(GPUDrawPushConstants), &push_constants);
  // vkCmdBindIndexBuffer(cmd, rectangle.indexBuffer.buffer, 0,
  //                      VK_INDEX_TYPE_UINT32);
  vkCmdBindIndexBuffer(cmd, testMeshes[2]->meshBuffers.indexBuffer.buffer, 0,
                       VK_INDEX_TYPE_UINT32);
  vkCmdDrawIndexed(cmd, testMeshes[2]->surfaces[0].count, 1,
                   testMeshes[2]->surfaces[0].startIndex, 0, 0);

  vkCmdEndRendering(cmd);
}

void VulkanEngine::init_default_data() {
  testMeshes = loadGltfMeshes(this, "../assets/basicmesh.glb").value();
}

// Global variables to store FPS history
std::vector<float> fpsHistory;
std::vector<float> frameTimeHistory;
const int historySize = 100;

void draw_fps_graph(bool useGraph = false) {
  // Set window size and flags
  ImGui::SetNextWindowSize(ImVec2(400, 250), ImGuiCond_Always);
  ImGui::SetNextWindowSizeConstraints(ImVec2(400, 250), ImVec2(400, 250));
  ImGui::Begin("FPS Display", nullptr,
               ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

  // Calculate FPS and frame time
  float fps = ImGui::GetIO().Framerate;
  float frameTime = 1000.0f / fps;

  // Update history
  fpsHistory.push_back(fps);
  frameTimeHistory.push_back(frameTime);
  if (fpsHistory.size() > historySize) {
    fpsHistory.erase(fpsHistory.begin());
    frameTimeHistory.erase(frameTimeHistory.begin());
  }

  if (!useGraph) {
    // Simple text version
    ImGui::Text("FPS: %.1f", fps);
    ImGui::Text("Frame Time: %.3f ms", frameTime);
  } else {
    // Graph version
    ImGui::PlotLines("FPS", fpsHistory.data(), fpsHistory.size(), 0, nullptr,
                     0.0f, 200.0f, ImVec2(0, 80));
    ImGui::PlotLines("Frame Time (ms)", frameTimeHistory.data(),
                     frameTimeHistory.size(), 0, nullptr, 0.0f, 33.3f,
                     ImVec2(0, 80));

    // Display current values
    ImGui::Text("Current FPS: %.1f", fps);
    ImGui::Text("Current Frame Time: %.3f ms", frameTime);
  }

  ImGui::End();
}

void VulkanEngine::run() {
  SDL_Event e;
  bool bQuit = false;

  // main loop
  while (!bQuit) {
    // Handle events on queue
    while (SDL_PollEvent(&e) != 0) {
      ImGui_ImplSDL2_ProcessEvent(&e);
      // close the window when user alt-f4s or clicks the X button
      if (e.type == SDL_QUIT)
        bQuit = true;

      if (e.type == SDL_WINDOWEVENT) {
        if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED &&
            e.window.data1 > 0 && e.window.data2 > 0) {
          resize_swapchain(e.window.data1, e.window.data2);
        }
      }

      if (e.type == SDL_WINDOWEVENT) {
        if (e.window.event == SDL_WINDOWEVENT_MINIMIZED) {
          stop_rendering = true;
        }
        if (e.window.event == SDL_WINDOWEVENT_RESTORED) {
          stop_rendering = false;
        }
      }
    }

    // do not draw if we are minimized
    if (stop_rendering) {
      // throttle the speed to avoid the endless spinning
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      continue;
    }

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
    {
      if (ImGui::Begin("background")) {
        ImGui::SliderFloat("Render Scale", &renderScale, 0.3f, 1.f);
        ComputeEffect &selected = backgroundEffects[currentBackgroundEffect];
        ImGui::Text("Selected effect: ", selected.name);
        ImGui::SliderInt("Effect Index", &currentBackgroundEffect, 0,
                         backgroundEffects.size() - 1);
        ImGui::InputFloat4("data1", (float *)&selected.data.data1);
        ImGui::InputFloat4("data2", (float *)&selected.data.data2);
        ImGui::InputFloat4("data3", (float *)&selected.data.data3);
        ImGui::InputFloat4("data4", (float *)&selected.data.data4);
      }
      ImGui::End();

      draw_fps_graph(true);
    }
    ImGui::Render();

    draw();
  }
}

void VulkanEngine::drawImgui(VkCommandBuffer cmd, VkImageView targetImageView) {
  VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(
      targetImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  VkRenderingInfo renderInfo =
      vkinit::rendering_info(_swapchainExtent, &colorAttachment, nullptr);

  vkCmdBeginRendering(cmd, &renderInfo);

  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

  vkCmdEndRendering(cmd);
}

void VulkanEngine::immediateSubmit(
    std::function<void(VkCommandBuffer cmd)> &&function) {
  VK_CHECK(vkResetFences(_device, 1, &_immFence));
  VK_CHECK(vkResetCommandBuffer(_immCommandBuffer, 0));

  auto cmd = _immCommandBuffer;
  VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(
      VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
  VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

  function(cmd);

  VK_CHECK(vkEndCommandBuffer(cmd));

  VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);
  VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo, nullptr, nullptr);

  // submit and wait for the completion fence
  // OPTIMIZATION: use a different queue to overlap work with the graphics queue
  VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submit, _immFence));
  VK_CHECK(vkWaitForFences(_device, 1, &_immFence, true, 9999999999));
}