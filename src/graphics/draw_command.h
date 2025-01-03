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

struct VisibilityResult {
    bool isVisible;
};

struct MeshDrawCommand {
    VkBuffer        indexBuffer;
    uint32_t        indexCount;
    VkDeviceAddress vertexBufferAddress;

    Mat4       worldFromLocal;
    MaterialId materialId;
};
