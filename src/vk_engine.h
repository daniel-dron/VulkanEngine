// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.h>
#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <memory>
#include <span>

#include "camera/camera.h"
#include "vk_descriptors.h"
#include "vk_loader.h"
#include "graphics/light.h"
#include <graphics/image_codex.h>
#include <graphics/pipelines/mesh_pipeline.h>
#include <graphics/pipelines/wireframe_pipeline.h>

#include "graphics/gfx_device.h"

class VulkanEngine;

struct ComputePushConstants {
	glm::vec4 data1;
	glm::vec4 data2;
	glm::vec4 data3;
	glm::vec4 data4;
};

struct ComputeEffect {
	const char* name;

	VkPipeline pipeline;
	VkPipelineLayout layout;

	ComputePushConstants data;
};

class VulkanEngine;

struct GltfMetallicRoughness {
	MaterialPipeline opaque_pipeline;
	MaterialPipeline transparent_pipeline;
	MaterialPipeline wireframe_pipeline;

	VkDescriptorSetLayout material_layout;

	struct MaterialConstants {
		glm::vec4 color_factors;
		glm::vec4 metal_rough_factors;
		// padding?
		glm::vec4 extra[14];
	};

	struct MaterialResources {
		ImageID color_image;
		VkSampler color_sampler;
		ImageID metal_rough_image;
		VkSampler metal_rough_sampler;
		ImageID normal_map;
		VkSampler normal_sampler;
		VkBuffer data_buffer;  // material constants
		uint32_t data_buffer_offset;
	};

	DescriptorWriter writer;

	void buildPipelines( VulkanEngine* engine );
	void clearResources( VkDevice device );

	MaterialInstance writeMaterial(
		VulkanEngine* engine, MaterialPass pass, const MaterialResources& resources,
		DescriptorAllocatorGrowable& descriptor_allocator );
};

struct RenderObject {
	uint32_t index_count;
	uint32_t first_index;
	VkBuffer index_buffer;

	MaterialInstance* material;
	Bounds bounds;
	glm::mat4 transform;
	VkDeviceAddress vertex_buffer_address;
};

struct DrawContext {
	std::vector<RenderObject> opaque_surfaces;
	std::vector<RenderObject> transparent_surfaces;
};

struct MeshNode final : public Node {
	std::shared_ptr<MeshAsset> mesh;

	void Draw( const glm::mat4& top_matrix, DrawContext& ctx ) override;
};

struct RendererOptions {
	bool wireframe = false;
	bool frustum = false;
	bool vsync = true;
};

/// @brief This is the main Vulkan Engine class
class VulkanEngine {
public:
	bool is_initialized{ false };
	int frame_number{ 0 };
	bool stop_rendering{ false };
	VkExtent2D window_extent{ 1920, 1080 };
	EngineStats stats;

	struct SDL_Window* window{ nullptr };

	static VulkanEngine& get( );

	/// @brief Does most of the engine initialization
	void init( );

	// shuts down the engine
	void cleanup( );

	// draw loop
	void draw( );

	// run main loop
	void run( );

	/// @brief Uploads mesh data to gpu buffers
	/// @param indices list of indices in uint32_t format
	/// @param vertices list of vertices
	/// @return returns a struct containing the allocated index and vertex buffer
	/// and its corresponding gpu address
	GPUMeshBuffers uploadMesh( std::span<uint32_t> indices,
		std::span<Vertex> vertices );

	std::unique_ptr<GfxDevice> gfx;
	bool dirt_swapchain = false;

	// vulkan stuff
	DeletionQueue main_deletion_queue;

	// Used as the color attachement for actual rendering
	// will be copied into the final swapchain image
	VkExtent2D draw_extent;
	float render_scale = 1.f;

	//
	// descriptors
	//
	DescriptorAllocatorGrowable global_descriptor_allocator;

	VkDescriptorSet draw_image_descriptors;
	VkDescriptorSetLayout draw_image_descriptor_layout;

	//
	// Pipeline
	//
	VkPipelineLayout gradient_pipeline_layout;
	MeshPipeline mesh_pipeline;
	WireframePipeline wireframe_pipeline;

	std::vector<ComputeEffect> background_effects;
	int current_background_effect{ 0 };

	// ----------
	// scene
	std::vector<PointLight> point_lights;
	GpuSceneData scene_data;
	AllocatedBuffer gpu_scene_data;
	VkDescriptorSetLayout gpu_scene_data_descriptor_layout;

	ImageID white_image;
	ImageID black_image;
	ImageID grey_image;
	ImageID error_checkerboard_image;

	VkSampler default_sampler_linear;
	VkSampler default_sampler_nearest;

	MaterialInstance default_data;
	GltfMetallicRoughness metal_rough_material;

	DrawContext main_draw_context;

	std::unordered_map<std::string, std::shared_ptr<LoadedGltf>> loaded_scenes;
	Camera3D camera;
	std::unique_ptr<FirstPersonFlyingController> fps_controller;
	CameraController* camera_controller;

	RendererOptions renderer_options;

	void resizeSwapchain( uint32_t width, uint32_t height );

private:
	/// @brief Initializes SDL context and creates SDL window
	void initSdl( );

	/// @brief Initializes core vulkan resources
	void initVulkan( );

	/// @brief Initializes swapchain.
	/// Creates an intermediate draw image where the actual frame rendering is
	/// done onto. The contents are then blipped to the swap chain image at the
	/// end of the frame. Also creates a depth image.

	/// @brief Creates pipeline descriptors and its allocators.
	/// Creates a growable descriptor allocator for each inflight frame.
	/// Creates a descriptor set for the background compute shader.
	/// Creates a descriptor set for scene data.
	void initDescriptors( );

	/// @brief Creates general pipelines.
	/// Creates the background compute pipeline.
	/// Creates the PBR Metalness pipeline.
	void initPipelines( );

	/// @brief Creates the background compute pipeline.
	void initBackgroundPipelines( );

	/// @brief Initializes ImGui entire context
	void initImgui( );

	/// @brief Dispatches compute shader to fill in main image
	/// @param cmd VkCommandBuffer that will queue in the work
	void drawBackground( VkCommandBuffer cmd );

	/// @brief Responsible for queueing commands to render all Renderables.
	/// Sorts based on material and performs frustum culling.
	/// @param cmd VkCommandBuffer that will queue in the work
	void drawGeometry( VkCommandBuffer cmd );

	void drawImgui( VkCommandBuffer cmd, VkImageView target_image_view );

	/// @brief Initializes default white, black, error images and its samplers
	void initDefaultData( );
	void initImages( );

	void initScene( );

	/// @brief Updates scene data and call Draw on each scene node.
	void updateScene( );

	void drawSceneHierarchy( );
};
