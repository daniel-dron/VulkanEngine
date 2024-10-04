#pragma once

#include "pipeline.h"

class ShadowMap : public Pipeline {
public:
	virtual Result<> init( GfxDevice& gfx ) override;
	virtual void cleanup( GfxDevice& gfx ) override;

	DrawStats draw( GfxDevice& gfx, VkCommandBuffer cmd, const std::vector<MeshDrawCommand>& draw_commands, const glm::mat4& projection, const glm::mat4 view, ImageID target ) const;
private:
	struct PushConstants {
		mat4 projection;
		mat4 view;
		mat4 model;
		VkDeviceAddress vertex_buffer_address;
	};

	void Reconstruct( GfxDevice& gfx );
};
