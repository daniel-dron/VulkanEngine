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
#include "graphics/light.h"
#include <graphics/image_codex.h>
#include <graphics/pipelines/mesh_pipeline.h>
#include <graphics/pipelines/wireframe_pipeline.h>

#include "graphics/gfx_device.h"
#include <graphics/material_codex.h>

#include <engine/scene.h>

class VulkanEngine;

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

	std::unique_ptr<GfxDevice> gfx;
	bool dirt_swapchain = false;

	// vulkan stuff
	DeletionQueue main_deletion_queue;

	// Used as the color attachement for actual rendering
	// will be copied into the final swapchain image
	VkExtent2D draw_extent;
	float render_scale = 1.f;

	//
	// Pipeline
	//
	VkPipelineLayout gradient_pipeline_layout;

	MaterialCodex material_codex;
	MeshPipeline mesh_pipeline;
	WireframePipeline wireframe_pipeline;

	// ----------
	// scene
	std::vector<PointLight> point_lights;
	GpuSceneData scene_data;
	GpuBuffer gpu_scene_data;

	ImageID white_image;
	ImageID black_image;
	ImageID grey_image;
	ImageID error_checkerboard_image;

	VkSampler default_sampler_linear;
	VkSampler default_sampler_nearest;

	std::vector<MeshDrawCommand> draw_commands;

	std::unordered_map<std::string, std::unique_ptr<Scene>> scenes;
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

	/// @brief Initializes ImGui entire context
	void initImgui( );

	/// @brief Responsible for queueing commands to render all Renderables.
	/// Sorts based on material and performs frustum culling.
	/// @param cmd VkCommandBuffer that will queue in the work
	void geometryPass( VkCommandBuffer cmd );

	void drawImgui( VkCommandBuffer cmd, VkImageView target_image_view );

	/// @brief Initializes default white, black, error images and its samplers
	void initDefaultData( );
	void initImages( );

	void initScene( );

	/// @brief Updates scene data and call Draw on each scene node.
	void updateScene( );
};
