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

#include <camera/camera.h>
#include <graphics/light.h>
#include <graphics/utils/vk_types.h>
#include <math/transform.h>

#include <memory>

#include "../graphics/resources/r_resources.h"

struct PointLight;

struct MeshAsset {
    u32 MeshIndex;
    u32 MaterialIndex;
};

struct Material {
    Vec4 BaseColor;
    f32  MetalnessFactor;
    f32  roughnessFactor;

    ImageId ColorId;
    ImageId MetalRoughnessId;
    ImageId NormalId;

    std::string Name;
};

struct Node {
    std::vector<MeshAsset>          MeshAssets;
    std::vector<TL::renderer::AABB> BoundingBoxes;

    std::string Name;

    Transform Transform;

    std::weak_ptr<Node>                Parent;
    std::vector<std::shared_ptr<Node>> Children;

    void SetTransform( const Mat4& newTransform );
    Mat4 GetTransformMatrix( ) const;
};

struct Scene {
    std::shared_ptr<Node> FindNodeByName( const std::string& name ) const;

    std::vector<TL::renderer::MaterialHandle> Materials;
    std::vector<TL::renderer::MeshHandle>     Meshes;
    std::vector<u32>                          FirstIndices;         // First index to be passed to the draw command.
    std::unique_ptr<TL::Buffer>               SceneBlobIndexBuffer; // This buffer contains all index buffers merged into one, futurally used for indirect draw calls.
    std::vector<std::shared_ptr<Node>>        TopNodes;
    std::vector<std::shared_ptr<Node>>        AllNodes;
    std::vector<Camera>                       Cameras;
    std::vector<PointLight>                   PointLights;
    std::vector<DirectionalLight>             DirectionalLights;

    std::string name;
};
