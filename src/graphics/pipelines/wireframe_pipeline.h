#pragma once

#include <vk_types.h>
#include <graphics/gfx_device.h>
#include <graphics/draw_command.h>
#include <expected>

class WireframePipeline {
public:
	enum class Error {
		ShaderLoadingFailed
	};

	struct PipelineError {
		Error error;
		std::string message;
	};

	template<typename T = void>
	using Result = std::expected<T, PipelineError>;

	Result<> init( GfxDevice& gfx );
	void cleanup( GfxDevice& gfx );

	DrawStats draw( GfxDevice& gfx, VkCommandBuffer cmd, const std::vector<MeshDrawCommand>& draw_commands, const GpuSceneData& scene_data ) const;

private:
	struct PushConstants {
		mat4 world_from_local;
		VkDeviceAddress vertex_buffer_address;
	};

	VkPipeline pipeline;
	VkPipelineLayout layout;
	VkDescriptorSetLayout material_layout;
	VkDescriptorSetLayout scene_data_layout;

	GpuBuffer gpu_scene_data;
};