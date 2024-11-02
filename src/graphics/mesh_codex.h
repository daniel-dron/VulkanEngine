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

namespace TL {
    class Buffer;
}

class TL_VkContext;

struct AABB {
    Vec3 min;
    Vec3 max;
};

struct GpuMesh {
    std::shared_ptr<TL::Buffer> indexBuffer;
    std::shared_ptr<TL::Buffer> vertexBuffer;

    AABB aabb;

    u32             indexCount;
    VkDeviceAddress vertexBufferAddress;
};

struct Mesh {
    struct Vertex {
        Vec3  position;
        float uvX;
        Vec3  normal;
        float uvY;
        Vec3  tangent;
        float pad;
        Vec3  biTangent;
        float pad2;
    };

    std::vector<Vertex> vertices;
    std::vector<u32>    indices;

    AABB aabb;
};

class MeshCodex {
public:
    void Cleanup( TL_VkContext& gfx );

    MeshId         AddMesh( TL_VkContext& gfx, const Mesh& mesh );
    const GpuMesh& GetMesh( MeshId id ) const;

private:
    GpuMesh              UploadMesh( TL_VkContext& gfx, const Mesh& mesh ) const;
    std::vector<GpuMesh> m_meshes;
};
