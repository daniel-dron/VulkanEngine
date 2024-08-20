#pragma once

#include <vk_types.h>
#include <graphics/gfx_device.h>
#include <graphics/draw_command.h>
#include <expected>

class Pipeline {
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

	virtual Result<> init( GfxDevice& gfx ) = 0;
	virtual void cleanup( GfxDevice& gfx ) = 0;

protected:
	VkPipeline pipeline;
	VkPipelineLayout layout;
};
