#pragma once

#include "pipeline.h"
#include <imgui.h>

class ImGuiPipeline : public Pipeline {
public:
	virtual Result<> init( GfxDevice& gfx ) override;
	void cleanup( GfxDevice& gfx ) override;

	void draw( GfxDevice& gfx, VkCommandBuffer cmd, ImDrawData* draw_data );
private:
	struct PushConstants {
		VkDeviceAddress vertex_buffer;
		uint32_t texture_id;
		uint32_t is_srgb;
		vec2 offset;
		vec2 scale;
	};

	ImageID font_texture_id;
	std::vector<GpuBuffer> index_buffers;
	std::vector<GpuBuffer> vertex_buffers;
};
