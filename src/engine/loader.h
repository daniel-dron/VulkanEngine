#pragma once

#include <vk_types.h>

#include <engine/scene.h>

class GfxDevice;
namespace fastgltf {
	class Asset;
}

class GltfLoader {
public:
	static std::unique_ptr<Scene> load( GfxDevice& gfx, const std::string& path );
};