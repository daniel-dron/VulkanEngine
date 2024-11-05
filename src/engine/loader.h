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

#include <graphics/utils/vk_types.h>
#include <engine/scene.h>

class TL_VkContext;

class GltfLoader {
public:
	static std::unique_ptr<Scene> Load( TL_VkContext& gfx, const std::string& path );
};