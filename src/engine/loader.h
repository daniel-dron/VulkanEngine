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

#include <engine/scene.h>
#include <graphics/utils/vk_types.h>
#include <world/tl_scene.h>


class TL_VkContext;

class GltfLoader {
public:
    static std::unique_ptr<Scene> Load( TL_VkContext& gfx, const std::string& path );

    static void LoadWorldFromGltf( const std::string& path, TL::world::World& world, TL::world::EntityHandle entity );
};