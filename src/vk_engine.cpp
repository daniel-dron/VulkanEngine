#include "vk_engine.h"

#include <SDL.h>
#include <SDL_vulkan.h>
#include <VkBootstrap.h>
#include <vk_images.h>
#include <vk_initializers.h>
#include <vk_pipelines.h>
#include <vk_types.h>
#include <vulkan/vulkan_core.h>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stack>
#include <thread>

#include "SDL_events.h"
#include "SDL_stdinc.h"
#include "SDL_video.h"
#include "fmt/core.h"
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/packing.hpp"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"
#include "vk_descriptors.h"

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#define TRACY_ENABLE
#include <glm/gtc/type_ptr.hpp>

#include "engine/input.h"
#include "imguizmo/ImGuizmo.h"
#include "tracy/TracyClient.cpp"
#include "tracy/tracy/Tracy.hpp"

#include <engine/loader.h>

VulkanEngine* loaded_engine = nullptr;

VulkanEngine& VulkanEngine::get( ) { return *loaded_engine; }

void VulkanEngine::init( ) {
	// only one engine initialization is allowed with the application.
	assert( loaded_engine == nullptr );
	loaded_engine = this;

	initSdl( );

	initVulkan( );

	initDefaultData( );

	initImgui( );

	gfx->swapchain.createImguiSet( );

	EG_INPUT.init( );

	initScene( );

	//const std::string structure_path = { "../../assets/sponza_scene.glb" };
	//const auto structure_file = loadGltf( this, structure_path );
	//assert( structure_file.has_value( ) );
	//loaded_scenes["structure"] = *structure_file;

	auto scene = GltfLoader::load( *gfx, "../../assets/sponza_scene.glb" );
	scenes["sponza"] = std::move( scene );

	// init camera
	fps_controller =
		std::make_unique<FirstPersonFlyingController>( &camera, 0.1f, 5.0f );
	camera_controller = fps_controller.get( );
	// camera.transform.set_position({0.0f, 0.0f, 100.0f});

	// everything went fine
	is_initialized = true;
}

void VulkanEngine::initSdl( ) {
	SDL_Init( SDL_INIT_VIDEO );

	constexpr auto window_flags =
		static_cast<SDL_WindowFlags>(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

	window = SDL_CreateWindow( "Vulkan Engine", SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED, window_extent.width,
		window_extent.height, window_flags );
}

void VulkanEngine::initVulkan( ) {
	gfx = std::make_unique<GfxDevice>( );

	if ( renderer_options.vsync ) {
		gfx->swapchain.present_mode = VK_PRESENT_MODE_FIFO_KHR;
	} else {
		gfx->swapchain.present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
	}

	gfx->init( window );

	main_deletion_queue.flush( );
}

void VulkanEngine::initImgui( ) {
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
		{VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000} };

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	pool_info.maxSets = 1000;
	pool_info.poolSizeCount = (uint32_t)std::size( pool_sizes );
	pool_info.pPoolSizes = pool_sizes;

	VkDescriptorPool imguiPool;
	VK_CHECK( vkCreateDescriptorPool( gfx->device, &pool_info, nullptr, &imguiPool ) );

	// 2: initialize imgui library

	// this initializes the core structures of imgui
	ImGui::CreateContext( );

	// this initializes imgui for SDL
	ImGui_ImplSDL2_InitForVulkan( window );

	// this initializes imgui for Vulkan
	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance = gfx->instance;
	init_info.PhysicalDevice = gfx->chosen_gpu;
	init_info.Device = gfx->device;
	init_info.Queue = gfx->graphics_queue;
	init_info.DescriptorPool = imguiPool;
	init_info.MinImageCount = 3;
	init_info.ImageCount = 3;
	init_info.UseDynamicRendering = true;

	// dynamic rendering parameters for imgui to use
	init_info.PipelineRenderingCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
	init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
	init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats =
		&gfx->swapchain.format;

	init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

	ImGui_ImplVulkan_Init( &init_info );

	ImGui_ImplVulkan_CreateFontsTexture( );

	auto& io = ImGui::GetIO( );
	io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	// add the destroy the imgui created structures
	main_deletion_queue.pushFunction( [&, imguiPool]( ) {
		ImGui_ImplVulkan_Shutdown( );
		vkDestroyDescriptorPool( gfx->device, imguiPool, nullptr );
	} );
}

void VulkanEngine::resizeSwapchain( uint32_t width, uint32_t height ) {
	vkDeviceWaitIdle( gfx->device );

	window_extent.width = width;
	window_extent.height = height;

	gfx->swapchain.recreate( width, height );
	gfx->swapchain.createImguiSet( );
}

void VulkanEngine::cleanup( ) {
	if ( is_initialized ) {
		// wait for gpu work to finish
		vkDeviceWaitIdle( gfx->device );

		mesh_pipeline.cleanup( *gfx );
		wireframe_pipeline.cleanup( *gfx );

		main_deletion_queue.flush( );

		gfx->cleanup( );

		SDL_DestroyWindow( window );
	}

	// clear engine pointer
	loaded_engine = nullptr;
}

void VulkanEngine::draw( ) {
	ZoneScopedN( "draw" );
	updateScene( );

	uint32_t swapchainImageIndex;
	{
		ZoneScopedN( "vsync" );
		// wait for last frame rendering phase. 1 sec timeout
		VK_CHECK( vkWaitForFences( gfx->device, 1, &gfx->swapchain.getCurrentFrame( ).fence, true,
			1000000000 ) );

		gfx->swapchain.getCurrentFrame( ).deletion_queue.flush( );

		VkResult e = (vkAcquireNextImageKHR( gfx->device, gfx->swapchain.swapchain, 1000000000,
			gfx->swapchain.getCurrentFrame( ).swapchain_semaphore,
			0, &swapchainImageIndex ));
		if ( e != VK_SUCCESS ) {
			return;
		}

		// Reset after acquire the next image from the swapchain
		// Incase of error, this fence would never get passed to the queue, thus
		// never triggering leaving us with a timeout next time we wait for fence
		VK_CHECK( vkResetFences( gfx->device, 1, &gfx->swapchain.getCurrentFrame( ).fence ) );
	}

	auto& color = gfx->image_codex.getImage( gfx->swapchain.getCurrentFrame( ).color );
	auto& depth = gfx->image_codex.getImage( gfx->swapchain.getCurrentFrame( ).depth );

	draw_extent.height = static_cast<uint32_t>(
		std::min( gfx->swapchain.extent.height, color.extent.height ) *
		render_scale);
	draw_extent.width = static_cast<uint32_t>(
		std::min( gfx->swapchain.extent.width, color.extent.width ) * render_scale);

	//
	// commands
	//
	auto cmd = gfx->swapchain.getCurrentFrame( ).command_buffer;

	// we can safely reset the command buffer because we waited for the render
	// fence
	VK_CHECK( vkResetCommandBuffer( cmd, 0 ) );

	// begin recording. will only use this command buffer once
	auto cmdBeginInfo = vkinit::command_buffer_begin_info(
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT );
	VK_CHECK( vkBeginCommandBuffer( cmd, &cmdBeginInfo ) );

	// transition our main draw image into general layout so it can
	// be written into
	vkutil::transition_image( cmd, color.image, VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL );
	vkutil::transition_image( cmd, depth.image, VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, true );

	geometryPass( cmd );

	// transform drawImg into source layout
	// transform swapchain img into dst layout
	vkutil::transition_image( cmd, color.image,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL );
	vkutil::transition_image( cmd, gfx->swapchain.images[swapchainImageIndex],
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL );

	vkutil::copy_image_to_image( cmd, color.image,
		gfx->swapchain.images[swapchainImageIndex],
		draw_extent, gfx->swapchain.extent );

	// transition swapchain to present
	vkutil::transition_image( cmd, gfx->swapchain.images[swapchainImageIndex],
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL );

	// draw imgui into the swapchain image
	drawImgui( cmd, gfx->swapchain.views[swapchainImageIndex] );

	// set swapchain image layout to Present so we can draw it
	vkutil::transition_image( cmd, gfx->swapchain.images[swapchainImageIndex],
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR );

	VK_CHECK( vkEndCommandBuffer( cmd ) );

	//
	// send commands
	//

	// wait on _swapchainSemaphore. signaled when the swap chain is ready
	// wait on _renderSemaphore. signaled when rendering has finished
	auto cmdinfo = vkinit::command_buffer_submit_info( cmd );

	auto waitInfo = vkinit::semaphore_submit_info(
		VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
		gfx->swapchain.getCurrentFrame( ).swapchain_semaphore );
	auto signalInfo = vkinit::semaphore_submit_info(
		VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, gfx->swapchain.getCurrentFrame( ).render_semaphore );

	auto submit = vkinit::submit_info( &cmdinfo, &signalInfo, &waitInfo );

	// submit command buffer and execute it
	// _renderFence will now block until the commands finish
	VK_CHECK( vkQueueSubmit2( gfx->graphics_queue, 1, &submit,
		gfx->swapchain.getCurrentFrame( ).fence ) );

	//
	// present
	//
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext = nullptr;
	presentInfo.pSwapchains = &gfx->swapchain.swapchain;
	presentInfo.swapchainCount = 1;

	// wait on _renderSemaphore, since we need the rendering to have finished
	// to display to the screen
	presentInfo.pWaitSemaphores = &gfx->swapchain.getCurrentFrame( ).render_semaphore;
	presentInfo.waitSemaphoreCount = 1;

	presentInfo.pImageIndices = &swapchainImageIndex;

	vkQueuePresentKHR( gfx->graphics_queue, &presentInfo );

	// increase frame number for next loop
	frame_number++;
}

void VulkanEngine::geometryPass( VkCommandBuffer cmd ) {
	ZoneScopedN( "Draw Geometry" );
	START_LABEL( cmd, "Draw Geometry", vec4( 1.0f, 0.0f, 0.0f, 1.0 ) );

	// reset counters
	stats.drawcall_count = 0;
	stats.triangle_count = 0;

	// begin clock
	auto start = std::chrono::system_clock::now( );

	// -----------
	// begin render frame
	{
		auto& color = gfx->image_codex.getImage( gfx->swapchain.getCurrentFrame( ).color );
		auto& depth = gfx->image_codex.getImage( gfx->swapchain.getCurrentFrame( ).depth );

		VkClearValue color_clear = { 0.0f, 0.0f, 0.0f, 1.0f };
		VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(
			color.view, &color_clear, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL );
		VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(
			depth.view, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL );

		VkRenderingInfo render_info =
			vkinit::rendering_info( draw_extent, &colorAttachment, &depthAttachment );
		vkCmdBeginRendering( cmd, &render_info );
	}

	DrawStats s = {};
	if ( renderer_options.wireframe ) {
		s = wireframe_pipeline.draw( *gfx, cmd, draw_commands, scene_data );
	} else {
		s = mesh_pipeline.draw( *gfx, cmd, draw_commands, scene_data );
	}
	stats.drawcall_count += s.drawcall_count;
	stats.triangle_count += s.triangle_count;

	vkCmdEndRendering( cmd );

	auto end = std::chrono::system_clock::now( );
	// convert to microseconds (integer), and then come back to miliseconds
	auto elapsed =
		std::chrono::duration_cast<std::chrono::microseconds>(end - start);
	stats.mesh_draw_time = elapsed.count( ) / 1000.f;

	END_LABEL( cmd );
}

void VulkanEngine::initDefaultData( ) {
	initImages( );

	gpu_scene_data = gfx->allocate( sizeof( GpuSceneData ), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, "drawGeometry" );

	main_deletion_queue.pushFunction( [=, this]( ) {
		gfx->free( gpu_scene_data );
	} );

	mesh_pipeline.init( *gfx );
	wireframe_pipeline.init( *gfx );
}

void VulkanEngine::initImages( ) {
	// 3 default textures, white, grey, black. 1 pixel each
	uint32_t white = glm::packUnorm4x8( glm::vec4( 1, 1, 1, 1 ) );
	white_image = gfx->image_codex.loadImageFromData( "debug_white_img", (void*)&white, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, false );

	uint32_t grey = glm::packUnorm4x8( glm::vec4( 0.66f, 0.66f, 0.66f, 1 ) );
	grey_image = gfx->image_codex.loadImageFromData( "debug_grey_img", (void*)&grey, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, false );

	uint32_t black = glm::packUnorm4x8( glm::vec4( 0, 0, 0, 0 ) );
	black_image = gfx->image_codex.loadImageFromData( "debug_black_img", (void*)&white, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, false );

	// checkerboard image
	uint32_t magenta = glm::packUnorm4x8( glm::vec4( 1, 0, 1, 1 ) );
	std::array<uint32_t, 16 * 16> pixels;  // for 16x16 checkerboard texture
	for ( int x = 0; x < 16; x++ ) {
		for ( int y = 0; y < 16; y++ ) {
			pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
		}
	}
	error_checkerboard_image = gfx->image_codex.loadImageFromData( "debug_checkboard_img", (void*)&white, VkExtent3D{ 16, 16, 1 }, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, false );

	VkSamplerCreateInfo sampl = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };

	sampl.magFilter = VK_FILTER_NEAREST;
	sampl.minFilter = VK_FILTER_NEAREST;

	vkCreateSampler( gfx->device, &sampl, nullptr, &default_sampler_nearest );

	sampl.magFilter = VK_FILTER_LINEAR;
	sampl.minFilter = VK_FILTER_LINEAR;
	vkCreateSampler( gfx->device, &sampl, nullptr, &default_sampler_linear );

	main_deletion_queue.pushFunction( [&]( ) {
		vkDestroySampler( gfx->device, default_sampler_nearest, nullptr );
		vkDestroySampler( gfx->device, default_sampler_linear, nullptr );
	} );
}

void VulkanEngine::initScene( ) {
	scene_data.ambient_light_color = vec3( 1.0f, 1.0f, 1.0f );
	scene_data.ambient_light_factor = 1.0f;

	scene_data.fog_color = vec4( 1.0f, 0.0f, 0.0f, 1.0f );
	scene_data.fog_end = 20.0f;
	scene_data.fog_start = 1.0f;

	PointLight light = {};
	light.transform.setPosition( vec3( 0.0f, 2.0f, 0.0f ) );
	light.color = vec4( 1.0f, 0.0f, 1.0f, 1000.0f );
	light.diffuse = 0.1f;
	light.specular = 1.0f;
	light.radius = 10.0f;

	point_lights.push_back( light );
}

// Global variables to store FPS history
std::vector<float> fpsHistory;
std::vector<float> frameTimeHistory;
const int historySize = 100;

void draw_fps_graph( bool useGraph = false ) {
	// Set window size and flags
	ImGui::SetNextWindowSize( ImVec2( 400, 250 ), ImGuiCond_Always );
	ImGui::SetNextWindowSizeConstraints( ImVec2( 400, 250 ), ImVec2( 400, 250 ) );
	ImGui::Begin( "FPS Display", nullptr,
		ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse );

	// Calculate FPS and frame time
	float fps = ImGui::GetIO( ).Framerate;
	float frameTime = 1000.0f / fps;

	// Update history
	fpsHistory.push_back( fps );
	frameTimeHistory.push_back( frameTime );
	if ( fpsHistory.size( ) > historySize ) {
		fpsHistory.erase( fpsHistory.begin( ) );
		frameTimeHistory.erase( frameTimeHistory.begin( ) );
	}

	if ( !useGraph ) {
		// Simple text version
		ImGui::Text( "FPS: %.1f", fps );
		ImGui::Text( "Frame Time: %.3f ms", frameTime );
	} else {
		// Graph version
		ImGui::PlotLines( "FPS", fpsHistory.data( ), (int)fpsHistory.size( ), 0,
			nullptr, 0.0f, 200.0f, ImVec2( 0, 80 ) );
		ImGui::PlotLines( "Frame Time (ms)", frameTimeHistory.data( ),
			(int)frameTimeHistory.size( ), 0, nullptr, 0.0f, 33.3f,
			ImVec2( 0, 80 ) );

		// Display current values
		ImGui::Text( "Current FPS: %.1f", fps );
		ImGui::Text( "Current Frame Time: %.3f ms", frameTime );
	}

	ImGui::End( );
}

void VulkanEngine::run( ) {
	bool bQuit = false;

	// main loop
	while ( !bQuit ) {
		FrameMarkNamed( "main" );
		// begin clock
		auto start = std::chrono::system_clock::now( );

		{
			ZoneScopedN( "poll_events" );
			EG_INPUT.poll_events( this );
		}

		if ( EG_INPUT.should_quit( ) ) {
			bQuit = true;
		}

		if ( EG_INPUT.was_key_pressed( EG_KEY::ESCAPE ) ) {
			bQuit = true;
		}

		static int savedMouseX, savedMouseY;
		// hide mouse
		if ( EG_INPUT.was_key_pressed( EG_KEY::MOUSE_RIGHT ) ) {
			SDL_GetMouseState( &savedMouseX, &savedMouseY );
			SDL_SetRelativeMouseMode( SDL_TRUE );
			SDL_ShowCursor( SDL_DISABLE );
		}
		// reveal mouse
		if ( EG_INPUT.was_key_released( EG_KEY::MOUSE_RIGHT ) ) {
			SDL_SetRelativeMouseMode( SDL_FALSE );
			SDL_ShowCursor( SDL_ENABLE );
			SDL_WarpMouseInWindow( window, savedMouseX, savedMouseY );
		}

		if ( dirt_swapchain ) {
			gfx->swapchain.recreate( gfx->swapchain.extent.width, gfx->swapchain.extent.height );
			gfx->swapchain.createImguiSet( );
			dirt_swapchain = false;
		}

		camera_controller->update( 1.0f / 165.0f );

		// do not draw if we are minimized
		if ( stop_rendering ) {
			// throttle the speed to avoid the endless spinning
			std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
			continue;
		}

		ImGui_ImplVulkan_NewFrame( );
		ImGui_ImplSDL2_NewFrame( );
		ImGuizmo::SetOrthographic( false );

		ImGui::NewFrame( );
		ImGuizmo::BeginFrame( );
		{
			// ----------
			// dockspace
			ImGui::DockSpaceOverViewport( 0, ImGui::GetMainViewport( ) );

			ImGui::ShowDemoWindow( );

			if ( ImGui::Begin( "Viewport", 0, ImGuiWindowFlags_NoScrollbar ) ) {
				ImGui::Image( (ImTextureID)(gfx->swapchain.getCurrentFrame( ).set),
					ImGui::GetWindowContentRegionMax( ) );

				// ----------
				// guizmos
			/*	if ( selected_node != nullptr ) {
					ImGuizmo::SetOrthographic( false );
					ImGuizmo::SetDrawlist( );
					ImGuizmo::SetRect( ImGui::GetWindowPos( ).x, ImGui::GetWindowPos( ).y,
						(float)ImGui::GetWindowWidth( ),
						(float)ImGui::GetWindowHeight( ) );
					auto camera_view = scene_data.view;
					auto camera_proj = scene_data.proj;
					camera_proj[1][1] *= -1;

					auto& tc = selected_node->worldTransform;
					ImGuizmo::Manipulate( glm::value_ptr( camera_view ),
						glm::value_ptr( camera_proj ),
						ImGuizmo::OPERATION::UNIVERSAL,
						ImGuizmo::MODE::LOCAL, glm::value_ptr( tc ) );
				}*/
			}
			ImGui::End( );

			if ( ImGui::Begin( "Scene" ) ) {
				//drawSceneHierarchy( );
			}
			ImGui::End( );

			if ( EG_INPUT.was_key_pressed( EG_KEY::Z ) ) {
				ImGui::OpenPopup( "Viewport Context" );
			}
			if ( ImGui::BeginPopup( "Viewport Context" ) ) {
				ImGui::SliderFloat( "Render Scale", &render_scale, 0.3f, 1.f );
				ImGui::Checkbox( "Wireframe", &renderer_options.wireframe );
				ImGui::Checkbox( "Frustum Culling", &renderer_options.frustum );
				if ( ImGui::Checkbox( "VSync", &renderer_options.vsync ) ) {
					if ( renderer_options.vsync ) {
						gfx->swapchain.present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
					} else {
						gfx->swapchain.present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
					}
					dirt_swapchain = true;
				}
				ImGui::EndPopup( );
			}

			if ( ImGui::Begin( "Settings" ) ) {
				//if ( selected_node != nullptr ) {
					//ImGui::SeparatorText( selected_node->name.c_str( ) );
					//MeshNode* node = dynamic_cast<MeshNode*>(selected_node);
					//if ( node ) {
					//	ImGui::TextColored( ImVec4( 0.7f, 0.2f, 0.5f, 1.0f ), "Mesh: %s",
					//		node->mesh->name.c_str( ) );
					//	static ImGuiTableFlags flags =
					//		ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg |
					//		ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable |
					//		ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable;

					//	// material data
					//	auto idx = 0;
					//	for ( auto& surface : node->mesh->surfaces ) {
					//		if ( ImGui::CollapsingHeader(
					//			std::format( "Surface {}", idx ).c_str( ) ) ) {
					//			ImGui::InputText( "Name", (char*)surface.material->name.c_str( ),
					//				surface.material->name.size( ),
					//				ImGuiInputTextFlags_ReadOnly );

					//			std::string pipeline_name;
					//			switch ( surface.material->data.passType ) {
					//			case MaterialPass::MainColor:
					//				pipeline_name = "Opaque Material";
					//				break;
					//			case MaterialPass::Transparent:
					//				pipeline_name = "Transparent Material";
					//				break;
					//			default:
					//				pipeline_name = "Other";
					//			}

					//			ImGui::InputText( "Pipeline", (char*)pipeline_name.c_str( ),
					//				pipeline_name.size( ),
					//				ImGuiInputTextFlags_ReadOnly );

					//			// albedo
					//			if ( ImGui::CollapsingHeader( "Albedo" ) ) {
					//				ImGui::Image( (ImTextureID)(
					//					surface.material->debug_sets.base_color_set),
					//					ImVec2( 200, 200 ) );
					//			}

					//			// metal roughness
					//			if ( ImGui::CollapsingHeader( "Metal Roughness" ) ) {
					//				ImGui::Image(
					//					(ImTextureID)(
					//						surface.material->debug_sets.metal_roughness_set),
					//					ImVec2( 200, 200 ) );
					//			}

					//			// normal map
					//			if ( ImGui::CollapsingHeader( "Normal" ) ) {
					//				ImGui::Image( (ImTextureID)(
					//					surface.material->debug_sets.normal_map_set),
					//					ImVec2( 200, 200 ) );
					//			}
					//		}
					//	}
					//}
				//}
				ImGui::SeparatorText( "Camera 3D" );
				camera.draw_debug( );

				ImGui::SeparatorText( "Camera Controller" );
				camera_controller->draw_debug( );

				ImGui::SeparatorText( "Light" );
				ImGui::ColorEdit3( "Ambient Color", &scene_data.ambient_light_color.x );
				ImGui::DragFloat( "Ambient Diffuse", &scene_data.ambient_light_factor,
					0.01f, 0.0f, 1.0f );

				ImGui::SeparatorText( "Point Lights" );
				for ( auto i = 0u; i < point_lights.size( ); i++ ) {
					if ( ImGui::CollapsingHeader(
						std::format( "Point Light {}", i ).c_str( ) ) ) {
						ImGui::PushID( i );
						ImGui::ColorEdit3( "Color", &point_lights.at( i ).color.x );

						auto pos = point_lights.at( i ).transform.getPosition( );
						ImGui::DragFloat3( "Pos", &pos.x, 0.1f );
						point_lights.at( i ).transform.setPosition( pos );

						ImGui::DragFloat( "Diffuse", &point_lights.at( i ).diffuse, 0.01f,
							0.0f, 1.0f );
						ImGui::DragFloat( "Specular", &point_lights.at( i ).specular, 0.01f,
							0.0f, 1.0f );
						ImGui::DragFloat( "Radius", &point_lights.at( i ).radius, 0.1f );
						ImGui::PopID( );
					}
				}

				ImGui::SeparatorText( "Fog" );
				ImGui::ColorEdit4( "Fog Color", &scene_data.fog_color.x );
				ImGui::DragFloat( "Start", &scene_data.fog_start, 0.1f, 0.1f );
				ImGui::DragFloat( "End", &scene_data.fog_end, 0.1f,
					scene_data.fog_start + 0.1f );
			}
			ImGui::End( );

			if ( ImGui::Begin( "Stats" ) ) {
				ImGui::Text( "frametime %f ms", stats.frametime );
				ImGui::Text( "draw time %f ms", stats.mesh_draw_time );
				ImGui::Text( "update time %f ms", stats.scene_update_time );
				ImGui::Text( "triangles %i", stats.triangle_count );
				ImGui::Text( "draws %i", stats.drawcall_count );
			}
			ImGui::End( );
		}

		ImGui::Render( );

		draw( );

		// get clock again, compare with start clock
		auto end = std::chrono::system_clock::now( );

		// convert to microseconds (integer), and then come back to miliseconds
		auto elapsed =
			std::chrono::duration_cast<std::chrono::microseconds>(end - start);
		stats.frametime = elapsed.count( ) / 1000.f;
	}
}

void VulkanEngine::drawImgui( VkCommandBuffer cmd,
	VkImageView target_image_view ) {
	VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(
		target_image_view, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL );
	VkRenderingInfo renderInfo =
		vkinit::rendering_info( gfx->swapchain.extent, &colorAttachment, nullptr );

	vkCmdBeginRendering( cmd, &renderInfo );

	ImGui_ImplVulkan_RenderDrawData( ImGui::GetDrawData( ), cmd );

	vkCmdEndRendering( cmd );
}

static void createDrawCommands( GfxDevice& gfx, const Scene& scene, const Scene::Node& node, std::vector<MeshDrawCommand>& draw_commands ) {
	if ( node.mesh_index != -1 ) {
		auto& mesh_asset = scene.meshes[node.mesh_index];

		size_t i = 0;
		for ( auto& primitive : mesh_asset.primitives ) {
			auto& mesh = gfx.mesh_codex.getMesh( primitive );

			MeshDrawCommand mdc = {
				.index_buffer = mesh.index_buffer.buffer,
				.index_count = mesh.index_count,
				.vertex_buffer_address = mesh.vertex_buffer_address,
				.world_from_local = node.transform.asMatrix( ),
				.material_id = scene.materials[mesh_asset.materials[i]]
			};
			draw_commands.push_back( mdc );

			i++;
		}
	}

	for ( auto& n : node.children ) {
		createDrawCommands( gfx, scene, *n.get( ), draw_commands );
	}
}

void VulkanEngine::updateScene( ) {
	ZoneScopedN( "update_scene" );
	auto start = std::chrono::system_clock::now( );

	//main_draw_context.opaque_surfaces.clear( );
	//main_draw_context.transparent_surfaces.clear( );
	//loaded_scenes["structure"]->Draw( glm::mat4{ 1.f }, main_draw_context );

	draw_commands.clear( );
	auto scene = scenes["sponza"].get( );
	createDrawCommands( *gfx.get( ), *scene, *(scenes["sponza"]->top_nodes[0].get( )), draw_commands );

	scene_data.view = camera.get_view_matrix( );
	// camera projection
	scene_data.proj = glm::perspective(
		glm::radians( 70.f ),
		(float)window_extent.width / (float)window_extent.height, 10000.f, 0.1f );

	scene_data.viewproj = scene_data.proj * scene_data.view;

	scene_data.camera_position = camera.transform.getPosition( );

	// point lights
	scene_data.number_of_lights = static_cast<int>(point_lights.size( ));
	for ( size_t i = 0; i < point_lights.size( ); i++ ) {
		auto& light = scene_data.point_lights[i];
		light.position = point_lights[i].transform.getPosition( );
		light.radius = point_lights[i].radius;
		light.color = point_lights[i].color;
		light.diffuse = point_lights[i].diffuse;
		light.specular = point_lights[i].specular;
	}

	auto end = std::chrono::system_clock::now( );
	// convert to microseconds (integer), and then come back to miliseconds
	auto elapsed =
		std::chrono::duration_cast<std::chrono::microseconds>(end - start);
	stats.scene_update_time = elapsed.count( ) / 1000.f;
}
