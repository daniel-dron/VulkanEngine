/******************************************************************************
******************************************************************************
**                                                                           **
**                             Twilight Engine                               **
**                                                                           **
**                  Copyright (c) 2024-present Daniel Dron                   **
**                                                                           **
**            This software is released under the MIT License.               **
**                 https://opensource.org/licenses/MIT                       **
**                                                                           **
******************************************************************************
******************************************************************************/

#pragma once

#include <vk_types.h>
#include <expected>

class GfxDevice;

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

	virtual Result<> Init( GfxDevice& gfx ) = 0;
	virtual void Cleanup( GfxDevice& gfx ) = 0;

protected:
	VkPipeline m_pipeline = nullptr;
	VkPipelineLayout m_layout = nullptr;
};
