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
#include <math/transform.h>
#include <vk_types.h>

#include <memory>

struct PointLight;

struct AABoundingBox {
    Vec3 min; // The point with the lowest coordinate in each cardinal direction (x, y, z)
    Vec3 max; // The point with the highest coordinate in each cardinal direction (x, y, z)
};

struct Node {
    std::vector<int> meshIds;
    std::vector<AABoundingBox> boundingBoxes;

    std::string name;
    int currentLod;

    Transform transform;

    std::weak_ptr<Node> parent;
    std::vector<std::shared_ptr<Node>> children;

    void SetTransform( const Mat4 &newTransform );
    Mat4 GetTransformMatrix( ) const;
};

struct Scene {
    struct MeshAsset {
        MeshId mesh;
        size_t material;
    };

    std::shared_ptr<Node> FindNodeByName( const std::string &name ) const;

    std::vector<MaterialId> materials;
    std::vector<MeshAsset> meshes;
    std::vector<std::shared_ptr<Node>> topNodes;
    std::vector<std::shared_ptr<Node>> allNodes;
    std::vector<Camera> cameras;
    std::vector<PointLight> pointLights;
    std::vector<DirectionalLight> directionalLights;

    std::string name;
};
