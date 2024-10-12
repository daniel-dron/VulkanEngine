#pragma once

#include <vk_types.h>
#include <engine/scene.h>

class GfxDevice;

class GltfLoader {
public:
	static std::unique_ptr<Scene> Load( GfxDevice& gfx, const std::string& path );
};