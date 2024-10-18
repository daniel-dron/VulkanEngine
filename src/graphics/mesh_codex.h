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

class GfxDevice;

struct AABB {
    Vec3 min;
    Vec3 max;
};

struct GpuMesh {
    // List of index buffers. One for each LOD level. Level 0 is the original mesh.
    // The amount of LODs is mesh dependent. Some bigger and more complex meshes will
    // have more LODs. Recommended to use std::min(indexBuffer.size() - 1, desiredLod)
    // to access the LOD safely
    std::vector<GpuBuffer> indexBuffer;
    GpuBuffer vertexBuffer;

    AABB aabb;

    // List of index count for each index buffer. One for each LOD level. Level 0 is the original mesh.
    std::vector<uint32_t> indexCount;
    VkDeviceAddress vertexBufferAddress;
};

struct Mesh {
    struct Vertex {
        Vec3 position;
        float uvX;
        Vec3 normal;
        float uvY;
        Vec3 tangent;
        float pad;
        Vec3 biTangent;
        float pad2;
    };

    std::vector<Vertex> vertices;
    std::vector<std::vector<uint32_t>> indices;

    AABB aabb;
};

class MeshCodex {
public:
    void Cleanup( GfxDevice &gfx ) const;

    MeshId AddMesh( GfxDevice &gfx, const Mesh &mesh );
    const GpuMesh &GetMesh( MeshId id ) const;

private:
    GpuMesh UploadMesh( GfxDevice &gfx, const Mesh &mesh ) const;
    std::vector<GpuMesh> m_meshes;
};
