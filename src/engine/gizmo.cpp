#include "gizmo.h"

#include "../vk_engine.h"
#include "../vk_initializers.h"
#include "../vk_loader.h"
#include "../vk_pipelines.h"
#include "../vk_types.h"
#include "vk_mem_alloc.h"

void GizmoRenderer::init(VulkanEngine* engine) {
  this->engine = engine;

  createPipeline();
  createDescriptors();
  loadArrowModel();
}

void GizmoRenderer::cleanup() {
  engine->destroyBuffer(cube.mesh_buffers.vertexBuffer);
  engine->destroyBuffer(cube.mesh_buffers.indexBuffer);

  vkDestroyDescriptorSetLayout(engine->device, descriptor_layout, nullptr);
  vkDestroyPipelineLayout(engine->device, layout, nullptr);
  vkDestroyPipeline(engine->device, pipeline, nullptr);
}

void GizmoRenderer::drawBasis(VkCommandBuffer cmd, VkDescriptorSet set,
                                 const mat4& world_from_local,
                                 const vec4& color) {
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1,
                          &set, 0, nullptr);

  GpuGizmoPushConstants push_constants;
  push_constants.world_from_local = world_from_local;
  push_constants.vertex_buffer_address = cube.mesh_buffers.vertexBufferAddress;
  vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                     sizeof(GpuGizmoPushConstants), &push_constants);

  vkCmdBindIndexBuffer(cmd, cube.mesh_buffers.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
  vkCmdDrawIndexed(cmd, cube.surfaces[0].count, 1,
                   cube.surfaces[0].start_index, 0, 0);
}

void GizmoRenderer::createPipeline() {
  // ----------
  // load gizmo shaders
  VkShaderModule frag_shader;
  if (!vkutil::loadShaderModule("../../shaders/gizmo.frag.spv", engine->device,
                                &frag_shader)) {
    fmt::println("Error when building gizmo fragment shader");
  }
  VkShaderModule vert_shader;
  if (!vkutil::loadShaderModule("../../shaders/gizmo.vert.spv", engine->device,
                                &vert_shader)) {
    fmt::println("Error when building gizmo vertex shader");
  }

  // ----------
  // push constants
  VkPushConstantRange push_constant_range{};
  push_constant_range.offset = 0;
  push_constant_range.size = sizeof(GpuGizmoPushConstants);
  push_constant_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

  // ----------
  // descriptor set layouts
  DescriptorLayoutBuilder layout_builder;
  layout_builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  descriptor_layout = layout_builder.build(
      engine->device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
      nullptr);

  // ----------
  // pipeline creation
  VkPipelineLayoutCreateInfo layout_info = vkinit::pipelineLayoutCreateInfo();
  layout_info.pSetLayouts = &descriptor_layout;
  layout_info.setLayoutCount = 1;
  layout_info.pPushConstantRanges = &push_constant_range;
  layout_info.pushConstantRangeCount = 1;
  VK_CHECK(
      vkCreatePipelineLayout(engine->device, &layout_info, nullptr, &layout));

  vkutil::PipelineBuilder pipelineBuilder;
  pipelineBuilder.set_shaders(vert_shader, frag_shader);
  pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
  pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
  pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
  pipelineBuilder.set_multisampling_none();
  pipelineBuilder.disable_blending();
  pipelineBuilder.disable_depthtest();

  // format
  pipelineBuilder.set_color_attachment_format(engine->draw_image.format);
  pipelineBuilder.set_depth_format(engine->depth_image.format);
  pipelineBuilder._pipelineLayout = layout;

  pipeline = pipelineBuilder.build_pipeline(engine->device);

  vkDestroyShaderModule(engine->device, frag_shader, nullptr);
  vkDestroyShaderModule(engine->device, vert_shader, nullptr);
}

void GizmoRenderer::createDescriptors() {}

void GizmoRenderer::loadArrowModel() {
  cube = loadMesh(engine, "../../assets/cube.glb", "Cube").value();
}