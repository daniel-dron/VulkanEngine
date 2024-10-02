// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.h>
#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <memory>
#include <span>

#include "camera/camera.h"
#include <graphics/descriptors.h>
#include "graphics/light.h"
#include <graphics/image_codex.h>
#include <graphics/pipelines/pbr_pipeline.h>
#include <graphics/pipelines/wireframe_pipeline.h>
#include <graphics/pipelines/gbuffer_pipeline.h>
#include <graphics/pipelines/imgui_pipeline.h>
#include <graphics/pipelines/skybox_pipeline.h>

#include "graphics/gfx_device.h"
#include <graphics/material_codex.h>

#include <engine/scene.h>
#include <graphics/ibl.h>

class VulkanEngine;

struct RendererOptions {
	bool wireframe = false;
	bool frustum = false;
	bool vsync = true;
	bool render_irradiance_instead_skybox = true;
};

/// @brief This is the main Vulkan Engine class
class VulkanEngine {
public:
	bool is_initialized{ false };
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
	PbrPipeline pbr_pipeline;
	WireframePipeline wireframe_pipeline;
	GBufferPipeline gbuffer_pipeline;
	ImGuiPipeline imgui_pipeline;
	SkyboxPipeline skybox_pipeline;

	struct PostProcessConfig {
		ImageID hdr;
		float gamma = 2.2f;
		float exposure = 1.0f;
	};
	mutable	PostProcessConfig pp_config;
	BindlessCompute post_process_pipeline;
	VkDescriptorSet post_process_set;


	// ----------
	// scene
	std::vector<PointLight> point_lights;
	std::vector<DirectionalLight> directional_lights;
	std::vector<GpuDirectionalLight> gpu_directional_lights;
	GpuSceneData scene_data;
	GpuBuffer gpu_scene_data;

	ImageID white_image;
	ImageID black_image;
	ImageID grey_image;
	ImageID error_checkerboard_image;

	IBL ibl;

	VkSampler default_sampler_linear;
	VkSampler default_sampler_nearest;

	std::vector<MeshDrawCommand> draw_commands;

	std::unordered_map<std::string, std::unique_ptr<Scene>> scenes;
	//Camera3D camera;
	std::unique_ptr<Camera> camera;
	std::unique_ptr<FirstPersonFlyingController> fps_controller;
	CameraController* camera_controller;

	RendererOptions renderer_options;

	void resizeSwapchain( uint32_t width, uint32_t height );

	float timer = 0;

private:
	/// @brief Initializes SDL context and creates SDL window
	void initSdl( );

	/// @brief Initializes core vulkan resources
	void initVulkan( );

	/// @brief Initializes ImGui entire context
	void initImgui( );

	void gbufferPass( VkCommandBuffer cmd ) const;
	void pbrPass( VkCommandBuffer cmd ) const;
	void skyboxPass( VkCommandBuffer cmd ) const;
	void postProcessPass( VkCommandBuffer cmd ) const;

	void drawImgui( VkCommandBuffer cmd, VkImageView target_image_view );

	/// @brief Initializes default white, black, error images and its samplers
	void initDefaultData( );
	void initImages( );

	void initScene( );

	/// @brief Updates scene data and call Draw on each scene node.
	void updateScene( );
};
