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

	DrawStats draw( GfxDevice& gfx, VkCommandBuffer cmd, const std::vector<OldMeshDrawCommand>& draw_commands, const GpuSceneData& scene_data ) const;

private:
	struct PushConstants {
		mat4 world_from_local;
		VkDeviceAddress scene_data_address;
		VkDeviceAddress vertex_buffer_address;
		uint32_t material_id;
	};

	VkPipeline pipeline;
	VkPipelineLayout layout;

	GpuBuffer gpu_scene_data;
};